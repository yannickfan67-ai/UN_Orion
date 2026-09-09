#include <stdint.h>
#include <stddef.h>
#include "net.h"

static uint32_t le32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int space(uint8_t c){return c==' '||c=='\t'||c=='\r'||c=='\n';}
static int digit(uint8_t c){return c>='0'&&c<='9';}
static int ppm_num(const uint8_t*d,size_t cap,size_t*p,uint32_t*out){while(*p<cap&&space(d[*p]))(*p)++;if(*p>=cap||!digit(d[*p]))return 0;uint32_t v=0;while(*p<cap&&digit(d[*p])){v=v*10u+(uint32_t)(d[(*p)++]-'0');if(v>8192)return 0;}*out=v;return 1;}
int net_http_get_bytes(const char*url,uint8_t*data,size_t cap,size_t*len,char*status,size_t status_cap){
    if(len)*len=0;if(!data||cap<64)return 0;if(!net_http_get(url,(char*)data,cap,status,status_cap))return 0;
    if(data[0]=='B'&&data[1]=='M'){uint32_t n=le32(data+2);if(n>=54&&n<cap){if(len)*len=n;return 1;}return 0;}
    if(data[0]=='P'&&data[1]=='6'&&space(data[2])){size_t p=2;uint32_t w,h,max;if(!ppm_num(data,cap,&p,&w)||!ppm_num(data,cap,&p,&h)||!ppm_num(data,cap,&p,&max)||max!=255||p>=cap||!space(data[p]))return 0;if(data[p]=='\r'&&p+1<cap&&data[p+1]=='\n')p+=2;else p++;size_t pixels=(size_t)w*(size_t)h;if(w&&pixels/w!=h)return 0;size_t total=p+pixels*3u;if(total<cap){if(len)*len=total;return 1;}return 0;}
    return 0;
}
