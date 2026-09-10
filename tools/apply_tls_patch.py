#!/usr/bin/env python3
from pathlib import Path
import re

ROOT=Path('.')

def read(p): return (ROOT/p).read_text()
def write(p,s):
    q=ROOT/p; q.parent.mkdir(parents=True,exist_ok=True); q.write_text(s)
def replace_once(s, old, new, label):
    if old not in s: raise SystemExit(f'missing marker: {label}')
    return s.replace(old,new,1)

write('include/tls.h', r'''#ifndef ORION_TLS_H
#define ORION_TLS_H
#include <stddef.h>
#include <stdint.h>
int tls_https_exchange(const char *host,uint16_t port,const uint8_t *request,size_t request_len,uint8_t *response,size_t response_cap,size_t *response_len,char *status,size_t status_cap);
int tls_platform_ready(char *status,size_t status_cap);
#endif
''')

write('kernel/libc.c', r'''#include <stddef.h>
void *memcpy(void *dst,const void *src,size_t n){unsigned char*d=dst;const unsigned char*s=src;while(n--)*d++=*s++;return dst;}
void *memmove(void *dst,const void *src,size_t n){unsigned char*d=dst;const unsigned char*s=src;if(d<s){while(n--)*d++=*s++;}else{d+=n;s+=n;while(n--)*--d=*--s;}return dst;}
int memcmp(const void *a,const void *b,size_t n){const unsigned char*x=a,*y=b;while(n--){if(*x!=*y)return *x<*y?-1:1;x++;y++;}return 0;}
size_t strlen(const char *s){size_t n=0;while(s&&s[n])n++;return n;}
''')

write('tools/fetch_bearssl.py', r'''#!/usr/bin/env python3
import hashlib, pathlib, shutil, sys, tarfile, urllib.request
URL='https://bearssl.org/bearssl-0.6.tar.gz'
SHA256='6705bba1714961b41a728dfc5debbe348d2966c117649392f8c8139efc83ff14'
out=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'build/vendor')
src=out/'bearssl-0.6'
stamp=src/'.orion-ready'
if stamp.exists(): raise SystemExit(0)
out.mkdir(parents=True,exist_ok=True)
archive=out/'bearssl-0.6.tar.gz'
if not archive.exists():
    with urllib.request.urlopen(URL,timeout=45) as r, archive.open('wb') as f: shutil.copyfileobj(r,f)
h=hashlib.sha256(archive.read_bytes()).hexdigest()
if h!=SHA256: raise SystemExit(f'BearSSL SHA256 mismatch: {h}')
with tarfile.open(archive,'r:gz') as tf:
    root=out.resolve()
    for m in tf.getmembers():
        dest=(out/m.name).resolve()
        if root not in dest.parents and dest!=root: raise SystemExit('unsafe tar path')
    tf.extractall(out,filter='data')
stamp.touch()
''')

write('kernel/tls.c', r'''#include <stddef.h>
#include <stdint.h>
#include <bearssl.h>
#include "io.h"
#include "net.h"
#include "serial.h"
#include "tls.h"
#include "orion_trust_anchors.c"

static br_ssl_client_context tls_sc;
static br_x509_minimal_context tls_xc;
static unsigned char tls_iobuf[BR_SSL_BUFSIZE_MONO];
static void scopy(char*d,size_t cap,const char*s){size_t i=0;if(!d||!cap)return;while(s&&s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static void cpuid1(uint32_t *ecx){uint32_t a=1,b,c=0,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(a),"c"(c));(void)b;(void)d;*ecx=c;}
static int entropy_bytes(uint8_t *out,size_t len){uint32_t ecx=0;cpuid1(&ecx);if(!(ecx&(1u<<30)))return 0;size_t o=0;while(o<len){unsigned long v=0;unsigned char ok=0;for(int tries=0;tries<16&&!ok;tries++)__asm__ volatile("rdrand %0; setc %1":"=r"(v),"=qm"(ok));if(!ok)return 0;for(size_t i=0;i<sizeof(v)&&o<len;i++,o++)out[o]=(uint8_t)(v>>(i*8));}return 1;}
static uint8_t cmos(uint8_t reg){outb(0x70,(uint8_t)(0x80u|reg));io_wait();return inb(0x71);}
static int rtc_snapshot(uint8_t *sec,uint8_t *min,uint8_t *hour,uint8_t *day,uint8_t *mon,uint8_t *year,uint8_t *cent,uint8_t *regb){for(int retry=0;retry<8;retry++){while(cmos(0x0A)&0x80u){}uint8_t s1=cmos(0),m1=cmos(2),h1=cmos(4),d1=cmos(7),mo1=cmos(8),y1=cmos(9),c1=cmos(0x32),b1=cmos(0x0B);while(cmos(0x0A)&0x80u){}uint8_t s2=cmos(0),m2=cmos(2),h2=cmos(4),d2=cmos(7),mo2=cmos(8),y2=cmos(9),c2=cmos(0x32),b2=cmos(0x0B);if(s1==s2&&m1==m2&&h1==h2&&d1==d2&&mo1==mo2&&y1==y2&&c1==c2&&b1==b2){*sec=s2;*min=m2;*hour=h2;*day=d2;*mon=mo2;*year=y2;*cent=c2;*regb=b2;return 1;}}return 0;}
static unsigned bcd(unsigned x){return (x&15u)+10u*((x>>4)&15u);}
static int leap(int y){return (y%4==0&&y%100!=0)||y%400==0;}
static int tls_time(uint32_t *days,uint32_t *seconds){uint8_t s,m,h,d,mo,y,c,b;if(!rtc_snapshot(&s,&m,&h,&d,&mo,&y,&c,&b))return 0;int pm=(h&0x80u)!=0;h&=0x7fu;if(!(b&0x04u)){s=(uint8_t)bcd(s);m=(uint8_t)bcd(m);h=(uint8_t)bcd(h);d=(uint8_t)bcd(d);mo=(uint8_t)bcd(mo);y=(uint8_t)bcd(y);c=(uint8_t)bcd(c);}if(!(b&0x02u)){if(pm&&h<12)h=(uint8_t)(h+12);if(!pm&&h==12)h=0;}int yy;if(c>=19&&c<=99)yy=(int)c*100+(int)y;else yy=y<70?2000+(int)y:1900+(int)y;if(yy<2020||yy>2199||mo<1||mo>12||d<1||d>31||h>23||m>59||s>60)return 0;static const uint8_t md[12]={31,28,31,30,31,30,31,31,30,31,30,31};uint32_t unix_days=0;for(int z=1970;z<yy;z++)unix_days+=(uint32_t)(leap(z)?366:365);for(int z=1;z<(int)mo;z++)unix_days+=(uint32_t)(md[z-1]+(z==2&&leap(yy)?1:0));unsigned maxd=md[mo-1]+(mo==2&&leap(yy)?1u:0u);if(d>maxd)return 0;unix_days+=(uint32_t)d-1u;*days=719528u+unix_days;*seconds=(uint32_t)h*3600u+(uint32_t)m*60u+(uint32_t)s;return 1;}
static int low_read(void *ctx,unsigned char *data,size_t len){(void)ctx;int n=net_tcp_read(data,len,10000);return n>0?n:-1;}
static int low_write(void *ctx,const unsigned char *data,size_t len){(void)ctx;size_t n=len>1200?1200:len;return net_tcp_write(data,n,6000)?(int)n:-1;}
int tls_platform_ready(char *status,size_t status_cap){uint8_t seed[8];uint32_t d,s;if(!entropy_bytes(seed,sizeof(seed))){scopy(status,status_cap,"TLS needs x86 RDRAND entropy");return 0;}if(!tls_time(&d,&s)){scopy(status,status_cap,"TLS needs a valid RTC clock");return 0;}return 1;}
int tls_https_exchange(const char *host,uint16_t port,const uint8_t *request,size_t request_len,uint8_t *response,size_t response_cap,size_t *response_len,char *status,size_t status_cap){if(response_len)*response_len=0;if(!host||!*host||!request||!response||response_cap<2)return 0;uint8_t seed[48];uint32_t days,seconds;if(!entropy_bytes(seed,sizeof(seed))){scopy(status,status_cap,"TLS entropy unavailable");return 0;}if(!tls_time(&days,&seconds)){scopy(status,status_cap,"TLS clock unavailable");return 0;}if(!net_tcp_connect(host,port,status,status_cap))return 0;br_ssl_client_init_full(&tls_sc,&tls_xc,TAs,TAs_NUM);br_ssl_engine_set_versions(&tls_sc.eng,BR_TLS12,BR_TLS12);br_ssl_engine_set_buffer(&tls_sc.eng,tls_iobuf,sizeof(tls_iobuf),0);br_ssl_engine_inject_entropy(&tls_sc.eng,seed,sizeof(seed));br_x509_minimal_set_time(&tls_xc,days,seconds);br_x509_minimal_set_minrsa(&tls_xc,256);br_ssl_engine_add_flags(&tls_sc.eng,BR_OPT_NO_RENEGOTIATION);if(!br_ssl_client_reset(&tls_sc,host,0)){net_tcp_close();scopy(status,status_cap,"TLS client reset failed");return 0;}br_sslio_context io;br_sslio_init(&io,&tls_sc.eng,low_read,0,low_write,0);if(br_sslio_write_all(&io,request,request_len)<0||br_sslio_flush(&io)<0){net_tcp_close();scopy(status,status_cap,"TLS handshake/write failed");return 0;}size_t used=0;for(;;){if(used+1>=response_cap){net_tcp_close();scopy(status,status_cap,"HTTPS response too large");return 0;}int n=br_sslio_read(&io,response+used,response_cap-1-used);if(n<0)break;used+=(size_t)n;}unsigned err=br_ssl_engine_last_error(&tls_sc.eng);net_tcp_close();if(err!=BR_ERR_OK){scopy(status,status_cap,"TLS validation or transport failed");return 0;}response[used]=0;if(response_len)*response_len=used;scopy(status,status_cap,"TLS 1.2 verified");serial_write("TLS: verified HTTPS session\r\n");return used>0;}
''')

write('docs/TLS.md', r'''# Native HTTPS / TLS

UN_Orion's native browser uses BearSSL 0.6 for TLS 1.2. The build downloads the exact BearSSL 0.6 release and verifies its SHA-256 before compiling it freestanding for the target architecture.

HTTPS is fail-closed: the server certificate chain, hostname and validity interval are validated against trust anchors generated from `TLS_CA_BUNDLE` (default `/etc/ssl/certs/ca-certificates.crt`). The native TLS layer requires a valid RTC clock and hardware RDRAND entropy; if either is unavailable, HTTPS is disabled rather than falling back to plaintext HTTP. HTTPS-to-HTTP redirects are rejected.

BearSSL is MIT licensed. The CA bundle remains governed by the licenses of the certificates/distribution that supplied it.

Limitations of this first native implementation: TLS 1.2 only; no TLS 1.3 yet, no certificate revocation/OCSP, one synchronous TCP/TLS session at a time, IPv4 only.
''')

net_h=read('include/net.h')
net_h=replace_once(net_h,'int net_ping_gateway(uint32_t timeout_ms);int net_http_get','int net_ping_gateway(uint32_t timeout_ms);int net_dns_lookup(const char *host,uint8_t out[4]);int net_tcp_connect(const char *host,uint16_t port,char *status,size_t status_cap);int net_tcp_read(uint8_t *data,size_t cap,uint32_t timeout_ms);int net_tcp_write(const uint8_t *data,size_t len,uint32_t timeout_ms);void net_tcp_close(void);int net_http_get','net public API')
write('include/net.h',net_h)

net=read('kernel/net.c')
net=replace_once(net,'#include "netdev.h"','#include "netdev.h"\n#include "tls.h"','tls include')
net=replace_once(net,'static uint16_t dns_id;\nstatic uint8_t dns_answer[4];','static uint16_t dns_id,dns_port;\nstatic uint8_t dns_answer[4];','dns state')
net=replace_once(net,'static uint32_t tcp_iss,tcp_snd_nxt,tcp_rcv_nxt;','static uint32_t tcp_iss,tcp_snd_nxt,tcp_snd_una,tcp_rcv_nxt;','tcp ack state')
old_udp=re.search(r'static void handle_udp\(.*?\n}\nstatic void handle_tcp',net,re.S)
if not old_udp: raise SystemExit('missing UDP handler')
new_udp=r'''static size_t dns_skip_name(const uint8_t*d,size_t n,size_t o){unsigned labels=0;while(o<n&&labels++<128){uint8_t z=d[o];if(z==0)return o+1;if((z&0xc0u)==0xc0u)return o+2;if((z&0xc0u)||z>63u||o+1u+(size_t)z>n)return n+1;o+=1u+(size_t)z;}return n+1;}
static void handle_udp(const uint8_t*p,size_t len){if(len<8)return;uint16_t sp=get16(p),dp=get16(p+2),ulen=get16(p+4);if(ulen<8||ulen>len||sp!=53||!dns_id||dp!=dns_port)return;const uint8_t*d=p+8;size_t n=(size_t)ulen-8u;if(n<12||get16(d)!=dns_id||(get16(d+2)&0x8000u)==0)return;if((get16(d+2)&0x000fu)!=0){dns_done=-1;return;}uint16_t qd=get16(d+4),an=get16(d+6);size_t o=12;for(unsigned q=0;q<qd;q++){o=dns_skip_name(d,n,o);if(o>n||o+4>n){dns_done=-1;return;}o+=4;}for(unsigned a=0;a<an;a++){o=dns_skip_name(d,n,o);if(o>n||o+10>n){dns_done=-1;return;}uint16_t typ=get16(d+o),cls=get16(d+o+2),rdl=get16(d+o+8);o+=10;if(o+(size_t)rdl>n){dns_done=-1;return;}if(typ==1&&cls==1&&rdl==4){memcopy(dns_answer,d+o,4);dns_done=1;return;}o+=(size_t)rdl;}dns_done=-1;}
static void handle_tcp'''
net=net[:old_udp.start()]+new_udp+net[old_udp.end():]
needle='if(hlen<20||hlen>len)return;\n    if((flags&0x12)==0x12'
net=replace_once(net,needle,'if(hlen<20||hlen>len)return;if((flags&0x10)&&ack>=tcp_snd_una&&ack<=tcp_snd_nxt)tcp_snd_una=ack;\n    if((flags&0x12)==0x12','tcp ack tracking')
old_dns=re.search(r'static int dns_lookup\(.*?\n}\nstatic int parse_url',net,re.S)
if not old_dns: raise SystemExit('missing dns_lookup')
new_dns=r'''static int dns_lookup(const char *host,uint8_t out[4]){if(parse_ipv4(host,out))return 1;if(!host||!*host)return 0;uint8_t q[512];memzero(q,sizeof(q));dns_id=(uint16_t)(0x4000+(timer_ticks()&0x3fff));put16(q,dns_id);put16(q+2,0x0100);put16(q+4,1);size_t o=12;const char*s=host;while(*s){size_t lp=o++;size_t n=0;if(o>=500)return 0;while(*s&&*s!='.'){if(n>=63||o>=500){dns_id=0;return 0;}q[o++]=(uint8_t)*s++;n++;}if(!n){dns_id=0;return 0;}q[lp]=(uint8_t)n;if(*s=='.')s++;}q[o++]=0;put16(q+o,1);o+=2;put16(q+o,1);o+=2;dns_done=0;dns_port=(uint16_t)(50000+(timer_ticks()%1000));uint32_t st=now_ms(),last=0xffffffffu;while(!elapsed(st,4000)){uint32_t now=now_ms();if(last==0xffffffffu||(uint32_t)(now-last)>800){send_udp(dns_ip,dns_port,53,q,(uint16_t)o);last=now;}net_poll();if(dns_done>0){memcopy(out,dns_answer,4);dns_id=0;dns_port=0;return 1;}if(dns_done<0)break;__asm__ volatile("pause");}dns_id=0;dns_port=0;return 0;}
int net_dns_lookup(const char *host,uint8_t out[4]){return ready&&dns_lookup(host,out);}
static uint8_t tcp_stream_buf[32768];
int net_tcp_connect(const char *host,uint16_t port,char *status,size_t status_cap){uint8_t dest[4];if(!ready||!host||!port){scopy(status,status_cap,"TCP unavailable");return 0;}if(!dns_lookup(host,dest)){scopy(status,status_cap,"DNS failed");return 0;}uint8_t nh[6];if(!resolve_next_hop(dest,nh)){(void)nh;scopy(status,status_cap,"ARP failed");return 0;}memcopy(tcp_peer_ip,dest,4);tcp_local_port=next_src_port++;if(next_src_port<49152)next_src_port=49152;tcp_peer_port=port;tcp_iss=0x13570000u+(uint32_t)timer_ticks();tcp_snd_nxt=tcp_iss;tcp_snd_una=tcp_iss;tcp_rcv_nxt=0;tcp_event=1;tcp_out=(char*)tcp_stream_buf;tcp_out_cap=sizeof(tcp_stream_buf);tcp_out_len=0;send_tcp(0x02,0,0);uint32_t st=now_ms();while(!elapsed(st,4000)&&tcp_event==1){net_poll();__asm__ volatile("pause");}if(tcp_event!=2){scopy(status,status_cap,"TCP connect failed");return 0;}return 1;}
int net_tcp_read(uint8_t *data,size_t cap,uint32_t timeout_ms){if(!data||!cap)return -1;uint32_t st=now_ms();for(;;){if(tcp_out_len){size_t n=tcp_out_len<cap?tcp_out_len:cap;memcopy(data,tcp_out,n);for(size_t i=n;i<tcp_out_len;i++)tcp_out[i-n]=tcp_out[i];tcp_out_len-=n;return (int)n;}if(tcp_event<0||tcp_event==4)return -1;if(elapsed(st,timeout_ms))return -1;net_poll();__asm__ volatile("pause");}}
int net_tcp_write(const uint8_t *data,size_t len,uint32_t timeout_ms){if(!data&&len)return 0;size_t off=0;while(off<len){size_t n=len-off;if(n>1200)n=1200;send_tcp(0x18,data+off,(uint16_t)n);uint32_t want=tcp_snd_nxt,st=now_ms();while(tcp_snd_una<want&&!elapsed(st,timeout_ms)&&tcp_event>=0){net_poll();__asm__ volatile("pause");}if(tcp_snd_una<want)return 0;off+=n;}return 1;}
void net_tcp_close(void){if(tcp_event>=2&&tcp_event!=4){send_tcp(0x11,0,0);uint32_t st=now_ms();while(!elapsed(st,300)&&tcp_event>=0&&tcp_event!=4){net_poll();__asm__ volatile("pause");}}tcp_event=0;tcp_out=0;tcp_out_cap=tcp_out_len=0;}
static int parse_url'''
net=net[:old_dns.start()]+new_dns+net[old_dns.end():]
old_parse=re.search(r'static int parse_url\(.*?\nstatic char lower_ascii',net,re.S)
if not old_parse: raise SystemExit('missing parse_url')
new_parse=r'''static int parse_url(const char*url,char*host,size_t hcap,uint16_t*port,char*path,size_t pcap,int*secure){if(!url||!host||hcap<2||!port||!path||pcap<2||!secure)return 0;const char*s=url;*secure=0;if(s[0]=='h'&&s[1]=='t'&&s[2]=='t'&&s[3]=='p'&&s[4]=='s'&&s[5]==':'&&s[6]=='/'&&s[7]=='/'){s+=8;*secure=1;*port=443;}else if(s[0]=='h'&&s[1]=='t'&&s[2]=='t'&&s[3]=='p'&&s[4]==':'&&s[5]=='/'&&s[6]=='/'){s+=7;*port=80;}else return 0;size_t hi=0;while(*s&&*s!='/'&&*s!=':'&&*s!='?'&&*s!='#'){if(hi+1>=hcap)return 0;host[hi++]=*s++;}host[hi]=0;if(!hi)return 0;if(*s==':'){s++;unsigned v=0,n=0;while(*s>='0'&&*s<='9'){v=v*10u+(unsigned)(*s++-'0');n++;if(v>65535)return 0;}if(!n||!v)return 0;*port=(uint16_t)v;}size_t pi=0;if(!*s){path[pi++]='/';}else if(*s=='?'||*s=='#'){path[pi++]='/';while(*s&&pi+1<pcap)path[pi++]=*s++;}else while(*s&&pi+1<pcap)path[pi++]=*s++;path[pi]=0;return *s==0;}
static char lower_ascii'''
net=net[:old_parse.start()]+new_parse+net[old_parse.end():]
old_red=re.search(r'/\* 1=resolved HTTP.*?\n}\nstatic int redirect_code',net,re.S)
if not old_red: raise SystemExit('missing redirect resolver')
new_red=r'''/* 1=resolved, -1=refused HTTPS downgrade, 0=unsupported/malformed. */
static int resolve_redirect(const char*base,const char*loc,char*out,size_t cap){if(!base||!loc||!*loc||!out||cap<9)return 0;char host[96],path[160];uint16_t port;int base_secure;if(!parse_url(base,host,sizeof(host),&port,path,sizeof(path),&base_secure))return 0;if(starts_ci(loc,"https://")){scopy(out,cap,loc);return 1;}if(starts_ci(loc,"http://")){if(base_secure)return -1;scopy(out,cap,loc);return 1;}if(loc[0]=='/'&&loc[1]=='/'){scopy(out,cap,base_secure?"https:":"http:");sappend(out,cap,loc);return 1;}for(size_t i=0;loc[i]&&loc[i]!='/'&&loc[i]!='?'&&loc[i]!='#';i++)if(loc[i]==':')return 0;scopy(out,cap,base_secure?"https://":"http://");sappend(out,cap,host);uint16_t def=(uint16_t)(base_secure?443:80);if(port!=def){sappend(out,cap,":");append_port(out,cap,port);}if(loc[0]=='/'){sappend(out,cap,loc);return 1;}char dir[160];size_t last=0;for(size_t i=0;path[i]&&path[i]!='?'&&path[i]!='#';i++)if(path[i]=='/')last=i;size_t n=last+1;if(n>=sizeof(dir))n=sizeof(dir)-1;for(size_t i=0;i<n;i++)dir[i]=path[i];dir[n]=0;if(!dir[0])scopy(dir,sizeof(dir),"/");sappend(out,cap,dir);sappend(out,cap,loc);return 1;}
static int redirect_code'''
net=net[:old_red.start()]+new_red+net[old_red.end():]
pos=net.find('int net_http_get(const char *url')
if pos<0: raise SystemExit('missing net_http_get')
new_http=r'''int net_http_get(const char *url,char *body,size_t cap,char *status,size_t status_cap){if(!ready||!body||cap<2)return 0;body[0]=0;if(status&&status_cap)status[0]=0;char current[256];scopy(current,sizeof(current),url);for(int redirect=0;redirect<4;redirect++){char host[96],path[160];uint16_t port;int secure;if(!parse_url(current,host,sizeof(host),&port,path,sizeof(path),&secure)){scopy(status,status_cap,"Invalid URL");return 0;}serial_write(secure?"HTTPS: GET ":"HTTP: GET ");serial_write(current);serial_write("\r\n");char req[768];size_t o=0;const char*a="GET ";while(*a&&o+1<sizeof(req))req[o++]=*a++;for(size_t i=0;path[i]&&o+1<sizeof(req);i++)req[o++]=path[i];a=" HTTP/1.0\r\nHost: ";while(*a&&o+1<sizeof(req))req[o++]=*a++;for(size_t i=0;host[i]&&o+1<sizeof(req);i++)req[o++]=host[i];uint16_t def=(uint16_t)(secure?443:80);if(port!=def&&o+7<sizeof(req)){req[o++]=':';char digits[6];int dn=0;unsigned v=port;do{digits[dn++]=(char)('0'+v%10u);v/=10u;}while(v&&dn<6);while(dn&&o+1<sizeof(req))req[o++]=digits[--dn];}a="\r\nUser-Agent: UN_Orion/0.0.8 UN_Vela/0.3.2 Aster/0.3.2\r\nAccept: text/html,text/plain;q=0.9,*/*;q=0.1\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n";while(*a&&o+1<sizeof(req))req[o++]=*a++;static char raw[32768];size_t raw_len=0;if(secure){if(!tls_https_exchange(host,port,(const uint8_t*)req,o,(uint8_t*)raw,sizeof(raw),&raw_len,status,status_cap))return 0;}else{if(!net_tcp_connect(host,port,status,status_cap))return 0;if(!net_tcp_write((const uint8_t*)req,o,5000)){net_tcp_close();scopy(status,status_cap,"HTTP write failed");return 0;}while(raw_len+1<sizeof(raw)){int n=net_tcp_read((uint8_t*)raw+raw_len,sizeof(raw)-1-raw_len,1400);if(n<0)break;raw_len+=(size_t)n;}net_tcp_close();if(!raw_len){scopy(status,status_cap,"No HTTP data");return 0;}raw[raw_len]=0;}size_t header_end=0,body_off=0;for(size_t i=0;i+3<raw_len;i++)if(raw[i]=='\r'&&raw[i+1]=='\n'&&raw[i+2]=='\r'&&raw[i+3]=='\n'){header_end=i;body_off=i+4;break;}if(!body_off)for(size_t i=0;i+1<raw_len;i++)if(raw[i]=='\n'&&raw[i+1]=='\n'){header_end=i;body_off=i+2;break;}if(status&&status_cap){size_t n=0;while(n<raw_len&&raw[n]!='\r'&&raw[n]!='\n'&&n+1<status_cap){status[n]=raw[n];n++;}status[n]=0;}if(!body_off){scopy(status,status_cap,"Malformed HTTP response");return 0;}int code=http_status_code(raw,header_end);if(redirect_code(code)){char loc[256];if(!header_value_ci(raw,header_end,"Location",loc,sizeof(loc))){scopy(status,status_cap,"Redirect missing Location");return 0;}if(redirect==3){scopy(status,status_cap,"Too many redirects");return 0;}char next[256];int rr=resolve_redirect(current,loc,next,sizeof(next));if(rr<0){scopy(status,status_cap,"Refusing HTTPS downgrade");return 0;}if(!rr){scopy(status,status_cap,"Unsupported redirect URL");return 0;}serial_write("HTTP: redirect ");serial_write(next);serial_write("\r\n");scopy(current,sizeof(current),next);continue;}const char*src=raw+body_off;size_t src_len=raw_len-body_off;if(src_len>=3&&(uint8_t)src[0]==0xEF&&(uint8_t)src[1]==0xBB&&(uint8_t)src[2]==0xBF){src+=3;src_len-=3;}size_t written;if(contains_ci(raw,header_end,"transfer-encoding: chunked")){written=decode_chunked(src,src_len,body,cap);if(!written&&src_len){scopy(status,status_cap,"Invalid chunked response");return 0;}}else{char clen[32];size_t expected=0;if(header_value_ci(raw,header_end,"Content-Length",clen,sizeof(clen))){if(!parse_size_dec(clen,&expected)){scopy(status,status_cap,"Invalid Content-Length");return 0;}if(src_len<expected){scopy(status,status_cap,"Truncated HTTP body");return 0;}if(src_len>expected)src_len=expected;}written=src_len<cap-1?src_len:cap-1;if(written)memcopy(body,src,written);body[written]=0;}serial_write(secure?"HTTPS: verified markup response received\r\n":"HTTP: markup response received\r\n");return 1;}scopy(status,status_cap,"Too many redirects");return 0;}
'''
net=net[:pos]+new_http
write('kernel/net.c',net)

vela_h=read('include/vela.h')
vela_h=vela_h.replace('#define VELA_VERSION "0.3.1-dev"','#define VELA_VERSION "0.3.2-dev"').replace('#define VELA_API_MINOR 3u','#define VELA_API_MINOR 4u')
if 'VELA_CAP_NAV_ACTIONS' not in vela_h:
    vela_h=replace_once(vela_h,'#define VELA_CAP_RESOURCES       (1ull << 8)','#define VELA_CAP_RESOURCES       (1ull << 8)\n#define VELA_CAP_NAV_ACTIONS     (1ull << 9)','vela cap')
    vela_h=replace_once(vela_h,'#define VELA_PROFILE_FULL VELA_FEATURE_JAVASCRIPT','#define VELA_PROFILE_FULL VELA_FEATURE_JAVASCRIPT\ntypedef enum VelaNavAction { VELA_NAV_BACK=1,VELA_NAV_FORWARD,VELA_NAV_RELOAD,VELA_NAV_LINE_UP,VELA_NAV_LINE_DOWN,VELA_NAV_PAGE_UP,VELA_NAV_PAGE_DOWN,VELA_NAV_HOME,VELA_NAV_END } VelaNavAction;','vela enum')
    vela_h=replace_once(vela_h,'int vela_can_back(void);int vela_can_forward(void);','int vela_can_back(void);int vela_can_forward(void);int vela_navigate_action(VelaNavAction action);','vela nav api')
write('include/vela.h',vela_h)
vela=read('kernel/vela.c')
if '#include "tls.h"' not in vela: vela=vela.replace('#include "vela.h"','#include "vela.h"\n#include "tls.h"',1)
vela=vela.replace('VELA_CAP_FEATURE_PROFILE|VELA_CAP_RESOURCES;','VELA_CAP_FEATURE_PROFILE|VELA_CAP_RESOURCES|VELA_CAP_NAV_ACTIONS;')
old_init='p.capabilities=VELA_PLATFORM_CAP_HTTP|VELA_PLATFORM_CAP_LOG|VELA_PLATFORM_CAP_RESOURCES;p.http_get=ohttp;p.log=olog;p.resource_get=ores;(void)vela_init_ex(w,&p,0);'
new_init='p.capabilities=VELA_PLATFORM_CAP_HTTP|VELA_PLATFORM_CAP_LOG|VELA_PLATFORM_CAP_RESOURCES;if(tls_platform_ready(0,0))p.capabilities|=VELA_PLATFORM_CAP_TLS;p.http_get=ohttp;p.log=olog;p.resource_get=ores;(void)vela_init_ex(w,&p,0);'
vela=replace_once(vela,old_init,new_init,'vela native platform TLS')
if 'int vela_navigate_action(' not in vela:
    anchor='int vela_reload(void){if(!g_url[0])return 0;return navigate(g_url,0);}'
    nav='int vela_navigate_action(VelaNavAction action){int before=g_scroll;int page=g_viewport_h>96?g_viewport_h-48:48;switch(action){case VELA_NAV_BACK:return vela_back();case VELA_NAV_FORWARD:return vela_forward();case VELA_NAV_RELOAD:return vela_reload();case VELA_NAV_LINE_UP:vela_scroll_by(-48);return g_scroll!=before;case VELA_NAV_LINE_DOWN:vela_scroll_by(48);return g_scroll!=before;case VELA_NAV_PAGE_UP:vela_scroll_by(-page);return g_scroll!=before;case VELA_NAV_PAGE_DOWN:vela_scroll_by(page);return g_scroll!=before;case VELA_NAV_HOME:vela_set_scroll(0);return g_scroll!=before;case VELA_NAV_END:vela_set_scroll(aster_document_height(&g_doc));return g_scroll!=before;default:return 0;}}'
    vela=replace_once(vela,anchor,anchor+'\n'+nav,'vela nav implementation')
write('kernel/vela.c',vela)
write('include/aster.h',read('include/aster.h').replace('#define ASTER_VERSION "0.3.1"','#define ASTER_VERSION "0.3.2"'))

mk=read('Makefile')
mk=mk.replace('VERSION := 0.0.7','VERSION := 0.0.8',1)
mk=mk.replace('KERNEL_CFLAGS := -target x86_64-unknown-none -ffreestanding','KERNEL_CFLAGS := -target x86_64-unknown-none -ffreestanding -fno-builtin',1)
mk=mk.replace('-Iinclude -I$(BUILD)/generated -DORION_ARCH_NAME','-Iinclude -I$(BUILD)/generated -I$(BUILD)/vendor/bearssl-0.6/inc -DORION_ARCH_NAME',1)
idx=mk.find('I686_CFLAGS :='); pre,tail=mk[:idx],mk[idx:]; tail=tail.replace('-Iinclude -I$(BUILD)/generated -DORION_ARCH_NAME','-Iinclude -I$(BUILD)/generated -I$(BUILD)/vendor/bearssl-0.6/inc -DORION_ARCH_NAME',1); mk=pre+tail
mk=mk.replace('kernel/net.c kernel/net_resource.c kernel/aster.c','kernel/net.c kernel/net_resource.c kernel/tls.c kernel/libc.c kernel/aster.c',1)
insert_make=r'''
BEARSSL_SRC := $(BUILD)/vendor/bearssl-0.6
BEARSSL_STAMP := $(BEARSSL_SRC)/.orion-ready
BEARSSL_X64 := $(BUILD)/libbearssl-x64.a
BEARSSL_I686 := $(BUILD)/libbearssl-i686.a
TLS_CA_BUNDLE ?= /etc/ssl/certs/ca-certificates.crt
TLS_ANCHORS := $(BUILD)/generated/orion_trust_anchors.c

$(BEARSSL_STAMP): tools/fetch_bearssl.py | $(BUILD)
	python3 tools/fetch_bearssl.py $(BUILD)/vendor

$(TLS_ANCHORS): $(BEARSSL_STAMP) | $(BUILD)
	test -r "$(TLS_CA_BUNDLE)"
	$(MAKE) -C $(BEARSSL_SRC) build/brssl CC=cc LD=cc
	mkdir -p $(BUILD)/generated
	$(BEARSSL_SRC)/build/brssl ta "$(TLS_CA_BUNDLE)" > $@
	test -s $@

$(BEARSSL_X64): $(BEARSSL_STAMP)
	rm -rf $(BUILD)/bearssl-x64 && mkdir -p $(BUILD)/bearssl-x64
	@set -e; for f in $$(find $(BEARSSL_SRC)/src -type f -name '*.c' | sort); do \
		o=$(BUILD)/bearssl-x64/$$(echo "$${f#$(BEARSSL_SRC)/}" | tr '/.' '__').o; \
		$(CC) $(KERNEL_CFLAGS) -I$(BEARSSL_SRC)/src -Wno-unused-parameter -c "$$f" -o "$$o"; \
	done
	llvm-ar rcs $@ $(BUILD)/bearssl-x64/*.o

$(BEARSSL_I686): $(BEARSSL_STAMP)
	rm -rf $(BUILD)/bearssl-i686 && mkdir -p $(BUILD)/bearssl-i686
	@set -e; for f in $$(find $(BEARSSL_SRC)/src -type f -name '*.c' | sort); do \
		o=$(BUILD)/bearssl-i686/$$(echo "$${f#$(BEARSSL_SRC)/}" | tr '/.' '__').o; \
		$(CC) $(I686_CFLAGS) -I$(BEARSSL_SRC)/src -Wno-unused-parameter -c "$$f" -o "$$o"; \
	done
	llvm-ar rcs $@ $(BUILD)/bearssl-i686/*.o
'''
mk=replace_once(mk,'.PHONY: all clean run image iso kernel smoke iso-smoke install-smoke network-smoke legacy-i686 legacy-smoke aster-smoke',insert_make+'\n.PHONY: all clean run image iso kernel smoke iso-smoke install-smoke network-smoke legacy-i686 legacy-smoke aster-smoke','make tls block')
mk=replace_once(mk,'$(BUILD)/arch.o: kernel/arch.S | $(BUILD)','$(BUILD)/tls.o: kernel/tls.c $(TLS_ANCHORS) $(BEARSSL_STAMP) | $(BUILD)\n\t$(CC) $(KERNEL_CFLAGS) -c $< -o $@\n\n$(BUILD)/arch.o: kernel/arch.S | $(BUILD)','x64 tls object')
mk=replace_once(mk,'$(BUILD)/kernel.elf: $(KERNEL_OBJS)\n\t$(LD) $(KERNEL_LDFLAGS) $^ -o $@','$(BUILD)/kernel.elf: $(KERNEL_OBJS) $(BEARSSL_X64)\n\t$(LD) $(KERNEL_LDFLAGS) $^ -o $@','x64 link')
mk=replace_once(mk,'$(LEGACY)/kernel/i686/entry.o: kernel/i686/entry.S | $(LEGACY)','$(LEGACY)/kernel/tls.o: kernel/tls.c $(TLS_ANCHORS) $(BEARSSL_STAMP) | $(LEGACY)\n\tmkdir -p $(@D)\n\t$(CC) $(I686_CFLAGS) -c $< -o $@\n\n$(LEGACY)/kernel/i686/entry.o: kernel/i686/entry.S | $(LEGACY)','i686 tls object')
mk=replace_once(mk,'$(LEGACY)/kernel32.elf: $(I686_OBJS) kernel/i686/linker.ld\n\t$(LD) $(I686_LDFLAGS) $(I686_OBJS) -o $@','$(LEGACY)/kernel32.elf: $(I686_OBJS) $(BEARSSL_I686) kernel/i686/linker.ld\n\t$(LD) $(I686_LDFLAGS) $(I686_OBJS) $(BEARSSL_I686) -o $@','i686 link')
write('Makefile',mk)

wf=read('.github/workflows/build.yml')
wf=wf.replace('sudo apt-get install -y clang lld llvm make qemu-system-x86 ovmf gnu-efi','sudo apt-get install -y clang lld llvm make qemu-system-x86 ovmf gnu-efi ca-certificates',1)
wf=wf.replace('QEMU network HTTP smoke test','QEMU network HTTP/TLS build smoke test',1)
wf=wf.replace('UN_Orion-v0.0.7-multi-firmware-media','UN_Orion-v0.0.8-multi-firmware-media',1).replace('UN_Orion-v0.0.7-install.iso','UN_Orion-v0.0.8-install.iso').replace('UN_Orion-v0.0.7-i686-bios.img','UN_Orion-v0.0.8-i686-bios.img')
write('.github/workflows/build.yml',wf)
print('native TLS patch applied')
