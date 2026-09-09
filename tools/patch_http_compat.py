from pathlib import Path

net = Path("kernel/net.c")
s = net.read_text()
start = s.index("int net_http_get(const char *url")
new_tail = r'''static int http_status_code(const char*raw,size_t len){
    size_t p=0;while(p<len&&raw[p]!=' ')p++;while(p<len&&raw[p]==' ')p++;int code=0,n=0;while(p<len&&raw[p]>='0'&&raw[p]<='9'&&n<3){code=code*10+(raw[p++]-'0');n++;}return n==3?code:0;
}
static int range_eq_ci(const char*a,size_t n,const char*b){size_t i=0;while(b[i])i++;if(i!=n)return 0;for(size_t j=0;j<n;j++)if(lower_ascii(a[j])!=lower_ascii(b[j]))return 0;return 1;}
static int header_value_ci(const char*raw,size_t n,const char*name,char*out,size_t cap){
    if(!raw||!out||!cap)return 0;out[0]=0;size_t p=0;while(p<n&&raw[p]!='\n')p++;if(p<n)p++;
    while(p<n){while(p<n&&(raw[p]=='\r'||raw[p]=='\n'))p++;if(p>=n)break;size_t a=p;while(p<n&&raw[p]!='\r'&&raw[p]!='\n')p++;size_t z=p,colon=a;while(colon<z&&raw[colon]!=':')colon++;if(colon<z&&range_eq_ci(raw+a,colon-a,name)){size_t v=colon+1;while(v<z&&(raw[v]==' '||raw[v]=='\t'))v++;while(z>v&&(raw[z-1]==' '||raw[z-1]=='\t'))z--;size_t o=0;while(v<z&&o+1<cap)out[o++]=raw[v++];out[o]=0;return 1;}}
    return 0;
}
static int parse_size_dec(const char*s,size_t*out){if(!s||!*s)return 0;size_t v=0;for(size_t i=0;s[i];i++){if(s[i]<'0'||s[i]>'9')return 0;size_t nv=v*10u+(size_t)(s[i]-'0');if(nv<v||nv>0x1000000u)return 0;v=nv;}*out=v;return 1;}
static void sappend(char*d,size_t cap,const char*s){size_t i=0;while(i<cap&&d[i])i++;while(i+1<cap&&s&&*s)d[i++]=*s++;if(i<cap)d[i]=0;}
static void append_port(char*d,size_t cap,uint16_t port){char t[8];int n=0;unsigned v=port;do{t[n++]=(char)('0'+v%10u);v/=10u;}while(v&&n<(int)sizeof(t));char q[9];int o=0;while(n)q[o++]=t[--n];q[o]=0;sappend(d,cap,q);}
static int starts_ci(const char*s,const char*p){while(*p){if(!*s||lower_ascii(*s++)!=lower_ascii(*p++))return 0;}return 1;}
/* 1=resolved HTTP, -1=HTTPS target needs TLS, 0=unsupported/malformed. */
static int resolve_redirect(const char*base,const char*loc,char*out,size_t cap){
    if(!base||!loc||!*loc||!out||cap<8)return 0;if(starts_ci(loc,"https://"))return -1;if(starts_ci(loc,"http://")){scopy(out,cap,loc);return 1;}if(loc[0]=='/'&&loc[1]=='/'){scopy(out,cap,"http:");sappend(out,cap,loc);return 1;}for(size_t i=0;loc[i]&&loc[i]!='/'&&loc[i]!='?'&&loc[i]!='#';i++)if(loc[i]==':')return 0;
    char host[96],path[160];uint16_t port;if(!parse_url(base,host,sizeof(host),&port,path,sizeof(path)))return 0;scopy(out,cap,"http://");sappend(out,cap,host);if(port!=80){sappend(out,cap,":");append_port(out,cap,port);}if(loc[0]=='/'){sappend(out,cap,loc);return 1;}char dir[160];size_t last=0;for(size_t i=0;path[i];i++)if(path[i]=='/')last=i;size_t n=last+1;if(n>=sizeof(dir))n=sizeof(dir)-1;for(size_t i=0;i<n;i++)dir[i]=path[i];dir[n]=0;if(!dir[0])scopy(dir,sizeof(dir),"/");sappend(out,cap,dir);sappend(out,cap,loc);return 1;
}
static int redirect_code(int c){return c==301||c==302||c==303||c==307||c==308;}

int net_http_get(const char *url,char *body,size_t cap,char *status,size_t status_cap){
    if(!ready||!body||cap<2)return 0;body[0]=0;if(status&&status_cap)status[0]=0;char current[256];scopy(current,sizeof(current),url);
    for(int redirect=0;redirect<4;redirect++){
        serial_write("HTTP: GET ");serial_write(current);serial_write("\r\n");char host[96],path[160];uint16_t port;if(!parse_url(current,host,sizeof(host),&port,path,sizeof(path))){scopy(status,status_cap,"Invalid URL");return 0;}uint8_t dest[4];if(!dns_lookup(host,dest)){scopy(status,status_cap,"DNS failed");return 0;}uint8_t nh[6];if(!resolve_next_hop(dest,nh)){(void)nh;scopy(status,status_cap,"ARP failed");return 0;}
        memcopy(tcp_peer_ip,dest,4);tcp_local_port=next_src_port++;tcp_peer_port=port;tcp_iss=0x13570000u+(uint32_t)timer_ticks();tcp_snd_nxt=tcp_iss;tcp_rcv_nxt=0;tcp_event=1;static char raw[32768];tcp_out=raw;tcp_out_cap=sizeof(raw)-1;tcp_out_len=0;send_tcp(0x02,0,0);uint32_t st=now_ms();while(!elapsed(st,3500)&&tcp_event==1){net_poll();__asm__ volatile("pause");}if(tcp_event!=2){scopy(status,status_cap,"TCP connect failed");return 0;}
        char req[640];size_t o=0;const char*a="GET ";while(*a&&o+1<sizeof(req))req[o++]=*a++;for(size_t i=0;path[i]&&o+1<sizeof(req);i++)req[o++]=path[i];a=" HTTP/1.0\r\nHost: ";while(*a&&o+1<sizeof(req))req[o++]=*a++;for(size_t i=0;host[i]&&o+1<sizeof(req);i++)req[o++]=host[i];if(port!=80&&o+7<sizeof(req)){req[o++]=':';char digits[6];int dn=0;unsigned v=port;do{digits[dn++]=(char)('0'+v%10);v/=10;}while(v&&dn<6);while(dn&&o+1<sizeof(req))req[o++]=digits[--dn];}a="\r\nUser-Agent: UN_Orion/0.0.7 UN_Vela/0.3.1 Aster/0.3.1\r\nAccept: text/html,text/plain;q=0.9,*/*;q=0.1\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n";while(*a&&o+1<sizeof(req))req[o++]=*a++;send_tcp(0x18,(const uint8_t*)req,(uint16_t)o);tcp_event=2;st=now_ms();uint32_t last_data=st;while(!elapsed(st,8000)){net_poll();if(tcp_event==3){last_data=now_ms();tcp_event=2;}if(tcp_event==4||tcp_event<0)break;if(tcp_out_len&&elapsed(last_data,1200))break;__asm__ volatile("pause");}raw[tcp_out_len]=0;if(!tcp_out_len){scopy(status,status_cap,"No HTTP data");return 0;}
        size_t header_end=0,body_off=0;for(size_t i=0;i+3<tcp_out_len;i++)if(raw[i]=='\r'&&raw[i+1]=='\n'&&raw[i+2]=='\r'&&raw[i+3]=='\n'){header_end=i;body_off=i+4;break;}if(!body_off)for(size_t i=0;i+1<tcp_out_len;i++)if(raw[i]=='\n'&&raw[i+1]=='\n'){header_end=i;body_off=i+2;break;}if(status&&status_cap){size_t n=0;while(n<tcp_out_len&&raw[n]!='\r'&&raw[n]!='\n'&&n+1<status_cap){status[n]=raw[n];n++;}status[n]=0;}if(!body_off){scopy(status,status_cap,"Malformed HTTP response");return 0;}
        int code=http_status_code(raw,header_end);if(redirect_code(code)){char loc[256];if(!header_value_ci(raw,header_end,"Location",loc,sizeof(loc))){scopy(status,status_cap,"Redirect missing Location");return 0;}if(redirect==3){scopy(status,status_cap,"Too many HTTP redirects");return 0;}char next[256];int rr=resolve_redirect(current,loc,next,sizeof(next));if(rr<0){scopy(status,status_cap,"Redirect requires HTTPS/TLS");return 0;}if(!rr){scopy(status,status_cap,"Unsupported redirect URL");return 0;}serial_write("HTTP: redirect ");serial_write(next);serial_write("\r\n");scopy(current,sizeof(current),next);continue;}
        const char*src=raw+body_off;size_t src_len=tcp_out_len-body_off;if(src_len>=3&&(uint8_t)src[0]==0xEF&&(uint8_t)src[1]==0xBB&&(uint8_t)src[2]==0xBF){src+=3;src_len-=3;}size_t written;if(contains_ci(raw,header_end,"transfer-encoding: chunked")){written=decode_chunked(src,src_len,body,cap);if(!written&&src_len){scopy(status,status_cap,"Invalid chunked response");return 0;}}else{char clen[32];size_t expected=0;if(header_value_ci(raw,header_end,"Content-Length",clen,sizeof(clen))){if(!parse_size_dec(clen,&expected)){scopy(status,status_cap,"Invalid Content-Length");return 0;}if(src_len<expected){scopy(status,status_cap,"Truncated HTTP body");return 0;}if(src_len>expected)src_len=expected;}written=src_len<cap-1?src_len:cap-1;if(written)memcopy(body,src,written);body[written]=0;}
        serial_write("HTTP: markup response received\r\n");return 1;
    }
    scopy(status,status_cap,"Too many HTTP redirects");return 0;
}
'''
net.write_text(s[:start] + new_tail)

p = Path("scripts/network_smoke.py")
t = p.read_text()
t = t.replace(
'''class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args): pass
''',
'''class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args): pass
    def do_GET(self):
        if self.path == '/':
            self.send_response(302)
            self.send_header('Location','/final.html')
            self.send_header('Content-Length','0')
            self.end_headers()
            return
        return super().do_GET()
''')
t = t.replace("(site/'index.html').write_text(", "(site/'final.html').write_text(")
t = t.replace(
"if 'HTTP test success' in text and 'VELA: title CI Orion HTTP' in text and 'HTTP: markup response received' in text:",
"if 'HTTP test success' in text and 'VELA: title CI Orion HTTP' in text and 'HTTP: redirect http://10.0.2.2:18080/final.html' in text and 'HTTP: markup response received' in text:")
t = t.replace(
"print('UN_Orion network/browser smoke passed: RTL8139 -> TCP -> HTTP markup -> UN_Vela -> Aster title parse')",
"print('UN_Orion network/browser smoke passed: RTL8139 -> TCP -> HTTP 302 redirect -> Content-Length body -> UN_Vela -> Aster title parse')")
p.write_text(t)
