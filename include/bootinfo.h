#ifndef ORION_BOOTINFO_H
#define ORION_BOOTINFO_H

#include <stdint.h>

typedef struct {
    uint64_t framebuffer_base;
    uint64_t framebuffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scanline;
    uint32_t pixel_format;

    uint64_t memory_map;
    uint64_t memory_map_size;
    uint64_t memory_descriptor_size;
    uint32_t memory_descriptor_version;
} OrionBootInfo;

#endif
