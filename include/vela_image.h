#ifndef UN_VELA_IMAGE_H
#define UN_VELA_IMAGE_H
#include <stddef.h>
#include <stdint.h>
typedef struct VelaImageInfo{uint32_t width,height;uint8_t channels;} VelaImageInfo;
int vela_image_probe(const uint8_t*,size_t,VelaImageInfo*);
int vela_image_decode_rgb24(const uint8_t*,size_t,uint8_t*,size_t,VelaImageInfo*);
#endif
