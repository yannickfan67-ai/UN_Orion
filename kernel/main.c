#include <stdint.h>
#include "bootinfo.h"

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

static void serial_putc(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0) {
    }
    outb(0x3F8, (uint8_t)c);
}

static void serial_write(const char *s) {
    while (*s) serial_putc(*s++);
}

static uint32_t pack_pixel(const OrionBootInfo *bi, uint32_t rgb) {
    uint32_t r = (rgb >> 16) & 0xFF;
    uint32_t g = (rgb >> 8) & 0xFF;
    uint32_t b = rgb & 0xFF;

    /* UEFI: 0 = RGBR, 1 = BGRR. On little-endian x86 the packed dword differs. */
    if (bi->pixel_format == 0) {
        return (b << 16) | (g << 8) | r;
    }
    return (r << 16) | (g << 8) | b;
}

static void put_pixel(OrionBootInfo *bi, uint32_t x, uint32_t y, uint32_t rgb) {
    if (!bi || x >= bi->width || y >= bi->height) return;
    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)bi->framebuffer_base;
    fb[(uint64_t)y * bi->pixels_per_scanline + x] = pack_pixel(bi, rgb);
}

static void fill_rect(OrionBootInfo *bi, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h, uint32_t rgb) {
    for (uint32_t yy = 0; yy < h; ++yy)
        for (uint32_t xx = 0; xx < w; ++xx)
            put_pixel(bi, x + xx, y + yy, rgb);
}

__attribute__((noreturn))
void kernel_main(OrionBootInfo *bi) {
    __asm__ volatile("cli");
    serial_init();
    serial_write("UN_Orion kernel alive\r\n");

    if (bi && bi->framebuffer_base) {
        fill_rect(bi, 0, 0, bi->width, bi->height, 0x10141A);

        uint32_t panel_w = bi->width > 900 ? 760 : (bi->width * 3 / 4);
        uint32_t panel_h = bi->height > 600 ? 360 : (bi->height / 2);
        uint32_t px = (bi->width - panel_w) / 2;
        uint32_t py = (bi->height - panel_h) / 2;

        fill_rect(bi, px, py, panel_w, panel_h, 0x1C2430);
        fill_rect(bi, px, py, panel_w, 44, 0x273142);
        fill_rect(bi, px + 28, py + 82, panel_w - 56, 2, 0x46556B);

        /* Temporary geometric boot mark until the font renderer lands. */
        uint32_t cx = bi->width / 2;
        uint32_t cy = bi->height / 2 + 10;
        fill_rect(bi, cx - 70, cy - 6, 140, 12, 0xE6EDF3);
        fill_rect(bi, cx - 6, cy - 70, 12, 140, 0xE6EDF3);
    }

    for (;;) {
        __asm__ volatile("hlt");
    }
}
