#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "aster.h"
#include "graphics.h"

void gfx_rect(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t rgb){(void)x;(void)y;(void)w;(void)h;(void)rgb;}
void gfx_line_h(uint32_t x,uint32_t y,uint32_t w,uint32_t rgb){(void)x;(void)y;(void)w;(void)rgb;}
uint32_t gfx_text(uint32_t x,uint32_t y,const char*s,uint32_t rgb,uint32_t scale){(void)x;(void)y;(void)s;(void)rgb;(void)scale;return 0;}
uint32_t gfx_display_text(uint32_t x,uint32_t y,const char*s,uint32_t rgb){(void)x;(void)y;(void)s;(void)rgb;return 0;}
int gfx_image_uri(uint32_t x,uint32_t y,uint32_t w,uint32_t h,const char*uri){(void)x;(void)y;(void)w;(void)h;(void)uri;return 1;}

static int has_text(const AsterDocument*d,const char*needle){
    for(unsigned i=0;i<d->paint_count;i++){
        const AsterPaintItem*p=&d->paint[i];
        if(!p->text_len)continue;
        char b[96];
        unsigned n=p->text_len<sizeof(b)-1?p->text_len:(unsigned)sizeof(b)-1;
        for(unsigned j=0;j<n;j++){
            b[j]=d->text[p->text_off+j];
        }
        b[n]=0;
        if(strstr(b,needle))return 1;
    }
    return 0;
}

int main(void){
    AsterDocument d;
    const char*html="<html><head><title>selectors</title><style>p{color:#123456}.notice{font-weight:bold}#hero{color:#654321}p.notice{font-size:24px}.accent,#other{text-decoration:underline}#gone{display:none}</style></head><body><p id='hero' class='notice accent'>Hello CSS</p><p id='other'>Other</p><p id='gone'>hidden id</p></body></html>";
    if(!aster_parse_html(&d,html))return 1;
    aster_layout(&d,320);
    if(d.css_count<7)return 2;
    int hero=0,other=0;
    for(unsigned i=0;i<d.paint_count;i++){
        const AsterPaintItem*p=&d.paint[i];
        if((p->flags&ASTER_PAINT_BOLD)&&(p->flags&ASTER_PAINT_UNDERLINE)&&p->color==0x654321&&p->scale==2)hero=1;
        if((p->flags&ASTER_PAINT_UNDERLINE)&&p->color==0x123456)other=1;
    }
    if(!hero||!other)return 3;
    if(has_text(&d,"hidden"))return 4;
    if(aster_api_version()!=((1u<<16)|3u))return 5;
    puts("Orion Aster selector smoke passed");
    return 0;
}
