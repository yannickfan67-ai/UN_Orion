#include <stdint.h>
#include "bootinfo.h"
#include "graphics.h"
#include "traf_font_22.h"
#include "traf_display_44.h"
static OrionBootInfo *g_bi;
static uint32_t pack_pixel(uint32_t rgb){
    uint32_t r=(rgb>>16)&255,g=(rgb>>8)&255,b=rgb&255;
    return g_bi->pixel_format==0 ? (b<<16)|(g<<8)|r : (r<<16)|(g<<8)|b;
}
void gfx_init(OrionBootInfo *bi){g_bi=bi;}
int gfx_ready(void){return g_bi&&g_bi->framebuffer_base!=0;}
uint32_t gfx_width(void){return g_bi?g_bi->width:0;}
uint32_t gfx_height(void){return g_bi?g_bi->height:0;}
void gfx_rect(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t c){
    if(!gfx_ready()||x>=g_bi->width||y>=g_bi->height)return;
    if(x+w>g_bi->width)w=g_bi->width-x; if(y+h>g_bi->height)h=g_bi->height-y;
    uint32_t pc=pack_pixel(c);
    for(uint32_t yy=0;yy<h;yy++){
        volatile uint32_t *fb=(volatile uint32_t*)(uintptr_t)g_bi->framebuffer_base+(uint64_t)(y+yy)*g_bi->pixels_per_scanline+x;
        for(uint32_t xx=0;xx<w;xx++)fb[xx]=pc;
    }
}
void gfx_line_h(uint32_t x,uint32_t y,uint32_t w,uint32_t c){gfx_rect(x,y,w,1,c);}
static uint32_t draw_char(uint32_t x,uint32_t y,char ch,uint32_t color,uint32_t scale){
    if(ch<TRAF_FIRST||ch>TRAF_LAST)ch='?'; unsigned gi=(unsigned)ch-TRAF_FIRST;
    for(uint32_t yy=0;yy<TRAF_H;yy++){uint32_t bits=traf_rows[gi][yy];for(uint32_t xx=0;xx<TRAF_W;xx++)if(bits&(1u<<(TRAF_W-1-xx)))gfx_rect(x+xx*scale,y+yy*scale,scale,scale,color);}
    return (traf_advance[gi]+1)*scale;
}
uint32_t gfx_text(uint32_t x,uint32_t y,const char*s,uint32_t c,uint32_t scale){uint32_t ox=x;while(s&&*s)x+=draw_char(x,y,*s++,c,scale);return x-ox;}
static uint32_t display_char(uint32_t x,uint32_t y,char ch,uint32_t color){
    if(ch<'A'||ch>'Z')return draw_char(x,y,ch,color,1); unsigned gi=(unsigned)(ch-'A');
    for(uint32_t yy=0;yy<TRAF_D_H;yy++){uint64_t bits=traf_d_rows[gi][yy];for(uint32_t xx=0;xx<TRAF_D_W;xx++)if(bits&(1ULL<<(TRAF_D_W-1-xx)))gfx_rect(x+xx,y+yy,1,1,color);}
    return traf_d_advance[gi]+2;
}
uint32_t gfx_display_text(uint32_t x,uint32_t y,const char*s,uint32_t c){uint32_t ox=x;while(s&&*s)x+=display_char(x,y,*s++,c);return x-ox;}
void gfx_text_right(uint32_t right,uint32_t y,const char*s,uint32_t c){
    uint32_t w=0; for(const char*p=s;*p;p++){char ch=*p;if(ch<32||ch>126)ch='?';w+=traf_advance[(unsigned)ch-32]+1;}
    gfx_text(right>w?right-w:0,y,s,c,1);
}
