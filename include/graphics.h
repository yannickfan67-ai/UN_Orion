#ifndef ORION_GRAPHICS_H
#define ORION_GRAPHICS_H
#include <stdint.h>
#include "bootinfo.h"
void gfx_init(OrionBootInfo *bi);
int gfx_ready(void);
uint32_t gfx_width(void);
uint32_t gfx_height(void);
void gfx_rect(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t rgb);
void gfx_line_h(uint32_t x,uint32_t y,uint32_t w,uint32_t rgb);
uint32_t gfx_text(uint32_t x,uint32_t y,const char *s,uint32_t rgb,uint32_t scale);
uint32_t gfx_display_text(uint32_t x,uint32_t y,const char *s,uint32_t rgb);
void gfx_text_right(uint32_t right,uint32_t y,const char *s,uint32_t rgb);
#endif
