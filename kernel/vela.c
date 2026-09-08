#include <stdint.h>
#include <stddef.h>
#include "vela.h"
#include "aster.h"
#include "net.h"
#include "serial.h"

static AsterDocument g_doc;
static char g_raw[32768];
static char g_url[192];
static char g_status[128];
static int g_url_len;
static int g_viewport=720;

static void copy(char*d,size_t cap,const char*s){size_t i=0;if(!cap)return;while(s&&s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static void append(char*d,size_t cap,const char*s){size_t i=0;while(i<cap&&d[i])i++;while(s&&*s&&i+1<cap)d[i++]=*s++;if(i<cap)d[i]=0;}
static char lower(char c){return c>='A'&&c<='Z'?(char)(c+32):c;}
static int starts_ci(const char*s,const char*p){while(*p){if(!*s||lower(*s++)!=lower(*p++))return 0;}return 1;}
static void parse_current(void){aster_parse_html(&g_doc,g_raw);aster_layout(&g_doc,g_viewport>40?g_viewport-40:g_viewport);}
static int canonicalize_url(const char*url,char*out,size_t cap){
    if(!url||!out||cap<9)return 0;while(*url==' '||*url=='\t')url++;if(!*url)return 0;
    if(starts_ci(url,"https://")){copy(g_status,sizeof(g_status),"HTTPS/TLS is not supported yet");return 0;}
    if(starts_ci(url,"http://")){copy(out,cap,url);return 1;}
    if(starts_ci(url,"file:")||starts_ci(url,"javascript:")||starts_ci(url,"data:")){copy(g_status,sizeof(g_status),"Unsupported URL scheme");return 0;}
    copy(out,cap,"http://");append(out,cap,url);return 1;
}

void vela_init(int viewport_width){
    g_viewport=viewport_width>120?viewport_width:120;g_url[0]=0;g_url_len=0;
    copy(g_status,sizeof(g_status),"Ready / Aster Engine ");append(g_status,sizeof(g_status),aster_version());
    copy(g_raw,sizeof(g_raw),
        "<html><head><title>UN_Vela</title></head><body>"
        "<header><h1>UN_Vela</h1><p>Native browser for UN_Orion.</p></header>"
        "<main><section><h2>Aster compatibility update</h2>"
        "<p>HTML is now passed to Aster as markup instead of being flattened by the HTTP transport.</p>"
        "<p>Common semantic containers, h3, blockquote, hr, span, script/style suppression and more entities are recognized.</p>"
        "<p>Enter an HTTP address above. Example: <code>10.0.2.2/</code></p>"
        "</section></main><footer><p>HTTP only for now; HTTPS/TLS is a future milestone.</p></footer>"
        "</body></html>");
    parse_current();
}
void vela_set_viewport(int viewport_width){if(viewport_width<120)viewport_width=120;if(viewport_width!=g_viewport){g_viewport=viewport_width;aster_layout(&g_doc,g_viewport>40?g_viewport-40:g_viewport);}}
void vela_input_char(char c){if(g_url_len<(int)sizeof(g_url)-1){g_url[g_url_len++]=c;g_url[g_url_len]=0;}}
void vela_backspace(void){if(g_url_len){g_url[--g_url_len]=0;}}
int vela_load_url(const char *url){
    char normalized[sizeof(g_url)];
    if(!url||!*url){copy(g_status,sizeof(g_status),"Enter an HTTP address");return 0;}
    if(!canonicalize_url(url,normalized,sizeof(normalized)))return 0;
    copy(g_url,sizeof(g_url),normalized);g_url_len=0;while(g_url[g_url_len]&&g_url_len<(int)sizeof(g_url)-1)g_url_len++;
    copy(g_status,sizeof(g_status),"Loading...");
    if(!net_http_get(g_url,g_raw,sizeof(g_raw),g_status,sizeof(g_status)))return 0;
    parse_current();
    serial_write("VELA: title ");serial_write(g_doc.title);serial_write("\r\n");
    if(g_doc.title[0]){char s[128];copy(s,sizeof(s),g_status);copy(g_status,sizeof(g_status),g_doc.title);append(g_status,sizeof(g_status)," / ");append(g_status,sizeof(g_status),s);}
    return 1;
}
int vela_go(void){return vela_load_url(g_url);}
const char *vela_url(void){return g_url;}
const char *vela_status(void){return g_status;}
const char *vela_title(void){return g_doc.title[0]?g_doc.title:VELA_NAME;}
void vela_paint(int x,int y,int width,int height){vela_set_viewport(width);aster_paint(&g_doc,x+18,y+10,width-36,height-20,0);}
