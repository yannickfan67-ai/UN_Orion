#include <stdint.h>
#include <stddef.h>
#include "graphics.h"
#include "vela.h"
#include "vela_image.h"

#define IMAGE_DATA_CAP (256u*1024u)
#define IMAGE_RGB_CAP (320u*240u*3u)
static uint8_t image_data[IMAGE_DATA_CAP];
static uint8_t image_rgb[IMAGE_RGB_CAP];

int gfx_image_uri(uint32_t x,uint32_t y,uint32_t w,uint32_t h,const char*uri){
    if(!uri||!w||!h)return 0;size_t n=0;char type[64],status[96];
    if(!vela_resource_get(uri,image_data,sizeof(image_data),&n,type,sizeof(type),status,sizeof(status)))return 0;
    VelaImageInfo info;if(!vela_image_probe(image_data,n,&info)||!info.width||!info.height)return 0;
    size_t need=(size_t)info.width*info.height*3u;if(need>sizeof(image_rgb)||!vela_image_decode_rgb24(image_data,n,image_rgb,sizeof(image_rgb),&info))return 0;
    if(w>640)w=640;if(h>480)h=480;
    for(uint32_t dy=0;dy<h;dy++){uint32_t sy=(uint32_t)(((uint64_t)dy*info.height)/h);if(sy>=info.height)sy=info.height-1;for(uint32_t dx=0;dx<w;dx++){uint32_t sx=(uint32_t)(((uint64_t)dx*info.width)/w);if(sx>=info.width)sx=info.width-1;size_t p=((size_t)sy*info.width+sx)*3u;uint32_t rgb=((uint32_t)image_rgb[p]<<16)|((uint32_t)image_rgb[p+1]<<8)|image_rgb[p+2];gfx_pixel(x+dx,y+dy,rgb);}}
    return 1;
}
