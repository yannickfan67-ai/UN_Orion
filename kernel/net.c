#include <stdint.h>
#include <stddef.h>
#include "serial.h"
#include "interrupts.h"
#include "net.h"
#include "netdev.h"

#define ETH_IP 0x0800
#define ETH_ARP 0x0806
#define IP_ICMP 1
#define IP_TCP 6
#define IP_UDP 17

static uint8_t mac[6];
static uint8_t ip_addr[4]={10,0,2,15};
static uint8_t netmask[4]={255,255,255,0};
static uint8_t gateway[4]={10,0,2,2};
static uint8_t dns_ip[4]={10,0,2,3};
static uint8_t gateway_mac[6];
static int gateway_mac_valid;
static int ready;
static uint64_t rx_count,tx_count;
static uint16_t ip_id=1;
static uint16_t next_src_port=49152;

static void memcopy(void*d,const void*s,size_t n){uint8_t*dd=d;const uint8_t*ss=s;while(n--)*dd++=*ss++;}
static void memzero(void*d,size_t n){uint8_t*p=d;while(n--)*p++=0;}
static int memequal(const void*a,const void*b,size_t n){const uint8_t*x=a,*y=b;while(n--)if(*x++!=*y++)return 0;return 1;}
static void scopy(char*d,size_t cap,const char*s){size_t i=0;if(!cap)return;while(s&&s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static uint32_t now_ms(void){uint32_t hz=timer_frequency();if(!hz)return 0;return (uint32_t)((timer_ticks()*1000ULL)/hz);}
static int elapsed(uint32_t start,uint32_t ms){return (uint32_t)(now_ms()-start)>=ms;}
static uint16_t checksum16(const void *data,size_t len){const uint8_t*p=data;uint32_t sum=0;while(len>1){sum+=((uint16_t)p[0]<<8)|p[1];p+=2;len-=2;}if(len)sum+=(uint16_t)p[0]<<8;while(sum>>16)sum=(sum&0xffff)+(sum>>16);return (uint16_t)~sum;}
static void put16(uint8_t*p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void put32(uint8_t*p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static uint16_t get16(const uint8_t*p){return ((uint16_t)p[0]<<8)|p[1];}
static uint32_t get32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}

static int eth_tx(const void *data,size_t len){
    if(!ready)return 0;
    if(netdev_tx(data,len)){tx_count++;return 1;}
    return 0;
}
static void eth_header(uint8_t*p,const uint8_t dst[6],uint16_t type){memcopy(p,dst,6);memcopy(p+6,mac,6);put16(p+12,type);}
static int same_subnet(const uint8_t a[4],const uint8_t b[4]){for(int i=0;i<4;i++)if((a[i]&netmask[i])!=(b[i]&netmask[i]))return 0;return 1;}
static int ip_equal(const uint8_t a[4],const uint8_t b[4]){return memequal(a,b,4);}

static void send_arp_request(const uint8_t target[4]){
    uint8_t p[42];static const uint8_t broad[6]={255,255,255,255,255,255};eth_header(p,broad,ETH_ARP);put16(p+14,1);put16(p+16,0x0800);p[18]=6;p[19]=4;put16(p+20,1);memcopy(p+22,mac,6);memcopy(p+28,ip_addr,4);memzero(p+32,6);memcopy(p+38,target,4);eth_tx(p,sizeof(p));
}
static void send_arp_reply(const uint8_t dstmac[6],const uint8_t dstip[4]){
    uint8_t p[42];eth_header(p,dstmac,ETH_ARP);put16(p+14,1);put16(p+16,0x0800);p[18]=6;p[19]=4;put16(p+20,2);memcopy(p+22,mac,6);memcopy(p+28,ip_addr,4);memcopy(p+32,dstmac,6);memcopy(p+38,dstip,4);eth_tx(p,sizeof(p));
}
static int resolve_next_hop(const uint8_t dest[4],uint8_t out[6]){
    const uint8_t *target=same_subnet(dest,ip_addr)?dest:gateway;
    if(ip_equal(target,gateway)&&gateway_mac_valid){memcopy(out,gateway_mac,6);return 1;}
    uint32_t st=now_ms(),last=0xffffffffu;while(!elapsed(st,2500)){
        uint32_t n=now_ms();if(last==0xffffffffu||(uint32_t)(n-last)>500){send_arp_request(target);last=n;}
        net_poll();if(ip_equal(target,gateway)&&gateway_mac_valid){memcopy(out,gateway_mac,6);return 1;}__asm__ volatile("pause");
    }return 0;
}
static size_t make_ipv4(uint8_t *p,uint8_t proto,const uint8_t dst[4],uint16_t payload_len){
    p[0]=0x45;p[1]=0;put16(p+2,(uint16_t)(20+payload_len));put16(p+4,ip_id++);put16(p+6,0x4000);p[8]=64;p[9]=proto;put16(p+10,0);memcopy(p+12,ip_addr,4);memcopy(p+16,dst,4);put16(p+10,checksum16(p,20));return 20;
}
static uint16_t tcp_checksum(const uint8_t src[4],const uint8_t dst[4],const uint8_t *tcp,size_t len){
    uint32_t sum=0;for(int i=0;i<4;i+=2){sum+=((uint16_t)src[i]<<8)|src[i+1];sum+=((uint16_t)dst[i]<<8)|dst[i+1];}sum+=IP_TCP;sum+=(uint16_t)len;for(size_t i=0;i+1<len;i+=2)sum+=((uint16_t)tcp[i]<<8)|tcp[i+1];if(len&1)sum+=(uint16_t)tcp[len-1]<<8;while(sum>>16)sum=(sum&0xffff)+(sum>>16);return (uint16_t)~sum;
}
static void send_ip_packet(uint8_t proto,const uint8_t dest[4],const uint8_t *payload,uint16_t plen){
    uint8_t frame[1600],nh[6];if(plen>1500-20)return;if(!resolve_next_hop(dest,nh))return;eth_header(frame,nh,ETH_IP);make_ipv4(frame+14,proto,dest,plen);memcopy(frame+34,payload,plen);eth_tx(frame,(size_t)34+plen);
}

/* synchronous transaction state used by early network services */
static volatile int icmp_reply;
static uint16_t icmp_ident=0x4f52,icmp_seq;
static volatile int dns_done;
static uint16_t dns_id;
static uint8_t dns_answer[4];
static volatile int tcp_event;
static uint8_t tcp_peer_ip[4];
static uint16_t tcp_local_port,tcp_peer_port;
static uint32_t tcp_iss,tcp_snd_nxt,tcp_rcv_nxt;
static char *tcp_out;static size_t tcp_out_cap,tcp_out_len;

static void send_icmp_echo(const uint8_t dest[4]){uint8_t p[16];memzero(p,sizeof(p));p[0]=8;put16(p+4,icmp_ident);put16(p+6,++icmp_seq);for(int i=8;i<16;i++)p[i]=(uint8_t)i;put16(p+2,checksum16(p,sizeof(p)));send_ip_packet(IP_ICMP,dest,p,sizeof(p));}
static void send_udp(const uint8_t dest[4],uint16_t sport,uint16_t dport,const uint8_t *data,uint16_t len){uint8_t p[1500];put16(p,sport);put16(p+2,dport);put16(p+4,(uint16_t)(8+len));put16(p+6,0);memcopy(p+8,data,len);send_ip_packet(IP_UDP,dest,p,(uint16_t)(8+len));}
static void send_tcp(uint8_t flags,const uint8_t *data,uint16_t len){
    uint8_t p[1500];memzero(p,20);put16(p,tcp_local_port);put16(p+2,tcp_peer_port);put32(p+4,tcp_snd_nxt);put32(p+8,tcp_rcv_nxt);p[12]=5<<4;p[13]=flags;put16(p+14,4096);if(data&&len)memcopy(p+20,data,len);put16(p+16,0);put16(p+16,tcp_checksum(ip_addr,tcp_peer_ip,p,(size_t)20+len));send_ip_packet(IP_TCP,tcp_peer_ip,p,(uint16_t)(20+len));if(flags&0x02)tcp_snd_nxt++;if(flags&0x01)tcp_snd_nxt++;tcp_snd_nxt+=len;
}

static void handle_arp(const uint8_t *p,size_t len){
    if(len<28||get16(p)!=1||get16(p+2)!=0x0800||p[4]!=6||p[5]!=4)return;uint16_t op=get16(p+6);const uint8_t *sm=p+8,*sip=p+14,*tip=p+24;
    if(ip_equal(sip,gateway)){memcopy(gateway_mac,sm,6);gateway_mac_valid=1;}
    if(op==1&&ip_equal(tip,ip_addr))send_arp_reply(sm,sip);
}
static void handle_icmp(const uint8_t *p,size_t len,const uint8_t srcip[4]){
    if(len<8)return;if(p[0]==0&&get16(p+4)==icmp_ident){icmp_reply=1;return;}if(p[0]==8){uint8_t r[256];if(len>sizeof(r))len=sizeof(r);memcopy(r,p,len);r[0]=0;r[2]=r[3]=0;put16(r+2,checksum16(r,len));send_ip_packet(IP_ICMP,srcip,r,(uint16_t)len);}
}
static void handle_udp(const uint8_t*p,size_t len){
    if(len<8)return;uint16_t sp=get16(p),dp=get16(p+2),ulen=get16(p+4);if(ulen<8||ulen>len)return;if(sp==53&&dp>=49152&&dns_id&&ulen>=20){const uint8_t*d=p+8;if(get16(d)!=dns_id)return;uint16_t qd=get16(d+4),an=get16(d+6);size_t o=12;for(unsigned q=0;q<qd&&o<ulen-8;q++){while(o<ulen-8&&d[o]){o+=(size_t)d[o]+1;}o+=5;}for(unsigned a=0;a<an&&o+12<=ulen-8;a++){if((d[o]&0xc0)==0xc0)o+=2;else{while(o<ulen-8&&d[o])o+=(size_t)d[o]+1;o++;}if(o+10>ulen-8)break;uint16_t typ=get16(d+o),cls=get16(d+o+2),rdl=get16(d+o+8);o+=10;if(o+rdl>ulen-8)break;if(typ==1&&cls==1&&rdl==4){memcopy(dns_answer,d+o,4);dns_done=1;return;}o+=rdl;}}
}
static void handle_tcp(const uint8_t*p,size_t len,const uint8_t srcip[4]){
    if(len<20||!memequal(srcip,tcp_peer_ip,4))return;uint16_t sp=get16(p),dp=get16(p+2);if(sp!=tcp_peer_port||dp!=tcp_local_port)return;uint32_t seq=get32(p+4),ack=get32(p+8);uint8_t hlen=(p[12]>>4)*4,flags=p[13];if(hlen<20||hlen>len)return;
    if((flags&0x12)==0x12&&tcp_event==1&&ack==tcp_snd_nxt){tcp_rcv_nxt=seq+1;tcp_event=2;send_tcp(0x10,0,0);return;}
    if(flags&0x04){tcp_event=-1;return;}
    size_t data_len=len-hlen;if(data_len&&seq==tcp_rcv_nxt){size_t room=tcp_out_cap>tcp_out_len?tcp_out_cap-tcp_out_len:0;size_t copy=data_len<room?data_len:room;if(copy){memcopy(tcp_out+tcp_out_len,p+hlen,copy);tcp_out_len+=copy;}tcp_rcv_nxt+=data_len;send_tcp(0x10,0,0);tcp_event=3;}
    if(flags&0x01){if(seq+data_len==tcp_rcv_nxt)tcp_rcv_nxt++;send_tcp(0x10,0,0);tcp_event=4;}
}
static void handle_ip(const uint8_t*p,size_t len){
    if(len<20||(p[0]>>4)!=4)return;uint8_t ihl=(p[0]&15)*4;if(ihl<20||ihl>len)return;uint16_t total=get16(p+2);if(total>len)total=(uint16_t)len;if(!ip_equal(p+16,ip_addr))return;const uint8_t*src=p+12,*pl=p+ihl;size_t plen=total-ihl;if(p[9]==IP_ICMP)handle_icmp(pl,plen,src);else if(p[9]==IP_UDP)handle_udp(pl,plen);else if(p[9]==IP_TCP)handle_tcp(pl,plen,src);
}
static void handle_frame(const uint8_t*p,size_t len){if(len<14)return;uint16_t t=get16(p+12);if(t==ETH_ARP)handle_arp(p+14,len-14);else if(t==ETH_IP)handle_ip(p+14,len-14);}

static void rx_dispatch(const uint8_t *frame,size_t len){handle_frame(frame,len);rx_count++;}
void net_poll(void){if(ready)netdev_poll(rx_dispatch);}
int net_init(void){
    if(!netdev_init(mac)){serial_write("NET: no supported adapter found\r\n");return 0;}
    ready=1;gateway_mac_valid=0;
    serial_write("NET: ");serial_write(netdev_name());serial_write(" ready MAC=");
    for(int i=0;i<6;i++){static const char h[]="0123456789ABCDEF";char x[4]={h[mac[i]>>4],h[mac[i]&15],i==5?'\r':':',0};serial_write(x);}
    serial_write("\n");send_arp_request(gateway);return 1;
}
int net_ready(void){return ready;}
const char *net_driver_name(void){return ready?netdev_name():"none";}
void net_get_mac(uint8_t out[6]){memcopy(out,mac,6);}void net_get_ipv4(uint8_t out[4]){memcopy(out,ip_addr,4);}void net_get_gateway(uint8_t out[4]){memcopy(out,gateway,4);}void net_configure(const uint8_t ip[4],const uint8_t mask[4],const uint8_t gw[4],const uint8_t dns[4]){memcopy(ip_addr,ip,4);memcopy(netmask,mask,4);memcopy(gateway,gw,4);memcopy(dns_ip,dns,4);gateway_mac_valid=0;send_arp_request(gateway);}uint64_t net_rx_packets(void){return rx_count;}uint64_t net_tx_packets(void){return tx_count;}
int net_ping_gateway(uint32_t timeout_ms){if(!ready)return 0;icmp_reply=0;send_icmp_echo(gateway);uint32_t st=now_ms();while(!elapsed(st,timeout_ms)){net_poll();if(icmp_reply)return 1;__asm__ volatile("pause");}return 0;}
static int parse_ipv4(const char*s,uint8_t out[4]){for(int i=0;i<4;i++){unsigned v=0,n=0;while(*s>='0'&&*s<='9'){v=v*10+(*s++-'0');n++;if(v>255)return 0;}if(!n)return 0;out[i]=(uint8_t)v;if(i<3){if(*s++!='.')return 0;}}return *s==0;}
static int dns_lookup(const char *host,uint8_t out[4]){
    if(parse_ipv4(host,out))return 1;uint8_t q[512];memzero(q,sizeof(q));dns_id=(uint16_t)(0x4000+(timer_ticks()&0x3fff));put16(q,dns_id);put16(q+2,0x0100);put16(q+4,1);size_t o=12;const char*s=host;while(*s){size_t lp=o++;size_t n=0;while(*s&&*s!='.'&&o<500){q[o++]=(uint8_t)*s++;n++;}q[lp]=(uint8_t)n;if(*s=='.')s++;}q[o++]=0;put16(q+o,1);o+=2;put16(q+o,1);o+=2;dns_done=0;uint16_t sport=(uint16_t)(50000+(timer_ticks()%1000));uint32_t st=now_ms(),last=0xffffffffu;while(!elapsed(st,3000)){uint32_t n=now_ms();if(last==0xffffffffu||(uint32_t)(n-last)>800){send_udp(dns_ip,sport,53,q,(uint16_t)o);last=n;}net_poll();if(dns_done){memcopy(out,dns_answer,4);dns_id=0;return 1;}__asm__ volatile("pause");}dns_id=0;return 0;
}
static int parse_url(const char*url,char*host,size_t hcap,uint16_t*port,char*path,size_t pcap){const char*s=url;if(s[0]=='h'&&s[1]=='t'&&s[2]=='t'&&s[3]=='p'&&s[4]==':'&&s[5]=='/'&&s[6]=='/')s+=7;size_t hi=0;while(*s&&*s!='/'&&*s!=':'&&hi+1<hcap)host[hi++]=*s++;host[hi]=0;*port=80;if(*s==':'){s++;unsigned v=0;while(*s>='0'&&*s<='9')v=v*10+(*s++-'0');if(!v||v>65535)return 0;*port=(uint16_t)v;}size_t pi=0;if(!*s){if(pcap>1){path[0]='/';path[1]=0;}}else while(*s&&pi+1<pcap)path[pi++]=*s++;path[pi]=0;return hi>0;}
static char lower_ascii(char c){return c>='A'&&c<='Z'?(char)(c+32):c;}
static int contains_ci(const char*s,size_t n,const char*needle){size_t nn=0;while(needle[nn])nn++;if(!nn||nn>n)return 0;for(size_t i=0;i+nn<=n;i++){size_t j=0;while(j<nn&&lower_ascii(s[i+j])==lower_ascii(needle[j]))j++;if(j==nn)return 1;}return 0;}
static int hexval(char c){if(c>='0'&&c<='9')return c-'0';c=lower_ascii(c);if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static size_t decode_chunked(const char*src,size_t len,char*out,size_t cap){
    size_t p=0,o=0;if(!cap)return 0;while(p<len){size_t chunk=0;int digits=0;while(p<len&&src[p]!='\r'&&src[p]!='\n'){if(src[p]==';'){while(p<len&&src[p]!='\r'&&src[p]!='\n')p++;break;}int h=hexval(src[p++]);if(h<0)return 0;chunk=chunk*16u+(size_t)h;digits++;if(chunk>0x100000u)return 0;}if(!digits)return 0;if(p<len&&src[p]=='\r')p++;if(p<len&&src[p]=='\n')p++;if(chunk==0){out[o]=0;return o;}if(chunk>len-p)return 0;size_t room=cap-1-o,copy=chunk<room?chunk:room;if(copy){memcopy(out+o,src+p,copy);o+=copy;}p+=chunk;if(p<len&&src[p]=='\r')p++;if(p<len&&src[p]=='\n')p++;}out[o]=0;return o;
}
int net_http_get(const char *url,char *body,size_t cap,char *status,size_t status_cap){
    if(!ready||!body||cap<2)return 0;serial_write("HTTP: GET ");serial_write(url);serial_write("\r\n");body[0]=0;if(status&&status_cap)status[0]=0;char host[96],path[160];uint16_t port;if(!parse_url(url,host,sizeof(host),&port,path,sizeof(path))){scopy(status,status_cap,"Invalid URL");return 0;}uint8_t dest[4];if(!dns_lookup(host,dest)){scopy(status,status_cap,"DNS failed");return 0;}uint8_t nh[6];if(!resolve_next_hop(dest,nh)){(void)nh;scopy(status,status_cap,"ARP failed");return 0;}
    memcopy(tcp_peer_ip,dest,4);tcp_local_port=next_src_port++;tcp_peer_port=port;tcp_iss=0x13570000u+(uint32_t)timer_ticks();tcp_snd_nxt=tcp_iss;tcp_rcv_nxt=0;tcp_event=1;static char raw[32768];tcp_out=raw;tcp_out_cap=sizeof(raw)-1;tcp_out_len=0;send_tcp(0x02,0,0);uint32_t st=now_ms();while(!elapsed(st,3500)&&tcp_event==1){net_poll();__asm__ volatile("pause");}if(tcp_event!=2){scopy(status,status_cap,"TCP connect failed");return 0;}
    char req[640];size_t o=0;const char*a="GET ";while(*a&&o+1<sizeof(req))req[o++]=*a++;for(size_t i=0;path[i]&&o+1<sizeof(req);i++)req[o++]=path[i];a=" HTTP/1.0\r\nHost: ";while(*a&&o+1<sizeof(req))req[o++]=*a++;for(size_t i=0;host[i]&&o+1<sizeof(req);i++)req[o++]=host[i];if(port!=80&&o+7<sizeof(req)){req[o++]=':';char digits[6];int dn=0;unsigned v=port;do{digits[dn++]=(char)('0'+v%10);v/=10;}while(v&&dn<6);while(dn&&o+1<sizeof(req))req[o++]=digits[--dn];}a="\r\nUser-Agent: UN_Vela/0.1 Aster/0.1.1\r\nAccept: text/html,text/plain;q=0.9,*/*;q=0.1\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n";while(*a&&o+1<sizeof(req))req[o++]=*a++;send_tcp(0x18,(const uint8_t*)req,(uint16_t)o);tcp_event=2;st=now_ms();uint32_t last_data=st;while(!elapsed(st,8000)){net_poll();if(tcp_event==3){last_data=now_ms();tcp_event=2;}if(tcp_event==4||tcp_event<0)break;if(tcp_out_len&&elapsed(last_data,1200))break;__asm__ volatile("pause");}raw[tcp_out_len]=0;if(!tcp_out_len){scopy(status,status_cap,"No HTTP data");return 0;}
    size_t header_end=0,body_off=0;for(size_t i=0;i+3<tcp_out_len;i++)if(raw[i]=='\r'&&raw[i+1]=='\n'&&raw[i+2]=='\r'&&raw[i+3]=='\n'){header_end=i;body_off=i+4;break;}if(!body_off)for(size_t i=0;i+1<tcp_out_len;i++)if(raw[i]=='\n'&&raw[i+1]=='\n'){header_end=i;body_off=i+2;break;}if(status&&status_cap){size_t n=0;while(n<tcp_out_len&&raw[n]!='\r'&&raw[n]!='\n'&&n+1<status_cap){status[n]=raw[n];n++;}status[n]=0;}if(!body_off){scopy(status,status_cap,"Malformed HTTP response");return 0;}
    const char*src=raw+body_off;size_t src_len=tcp_out_len-body_off;if(src_len>=3&&(uint8_t)src[0]==0xEF&&(uint8_t)src[1]==0xBB&&(uint8_t)src[2]==0xBF){src+=3;src_len-=3;}size_t written;if(contains_ci(raw,header_end,"transfer-encoding: chunked")){written=decode_chunked(src,src_len,body,cap);if(!written&&src_len){scopy(status,status_cap,"Invalid chunked response");return 0;}}else{written=src_len<cap-1?src_len:cap-1;if(written)memcopy(body,src,written);body[written]=0;}
    serial_write("HTTP: markup response received\r\n");return 1;
}
