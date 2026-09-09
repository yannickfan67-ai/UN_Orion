#ifndef ORION_INTERRUPTS_H
#define ORION_INTERRUPTS_H
#include <stdint.h>
void arch_gdt_init(void);
void interrupts_init(uint32_t pit_hz);
uint64_t timer_ticks(void);
uint32_t timer_frequency(void);
int keyboard_pop_scancode(uint8_t *scan);
int mouse_pop_byte(uint8_t *byte);
int mouse_available(void);
int mouse_packet_size(void);
__attribute__((noreturn)) void orion_exception_panic(uint64_t vector,uint64_t error,uint64_t rip,uint64_t cs,uint64_t rflags);
#endif
