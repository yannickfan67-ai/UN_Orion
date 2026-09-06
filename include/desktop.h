#ifndef ORION_DESKTOP_H
#define ORION_DESKTOP_H
#include <stdint.h>
#include "bootinfo.h"
void desktop_init(OrionBootInfo *bi);
void desktop_key_scancode(uint8_t scan);
void desktop_mouse_byte(uint8_t byte);
void desktop_tick(void);
#endif
