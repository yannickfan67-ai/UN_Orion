#include <stdint.h>
#include "io.h"
#include "serial.h"
#include "graphics.h"
#include "interrupts.h"

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_DATA 0x21
#define PIC2_DATA 0xA1
#define PIT_CH0 0x40
#define PIT_CMD 0x43
#define PIT_BASE_HZ 1193182U

typedef struct __attribute__((packed)){uint16_t off0,sel;uint8_t zero,attr;uint16_t off1;} IdtGate32;
struct __attribute__((packed)) IdtPtr32{uint16_t limit;uint32_t base;};
struct __attribute__((packed)) GdtPtr32{uint16_t limit;uint32_t base;};
struct I686IntFrame{uint32_t eip,cs,eflags,esp,ss;};
static IdtGate32 idt[256];static uint64_t gdt[3]={0,0x00CF9A000000FFFFULL,0x00CF92000000FFFFULL};extern void orion_load_gdt(struct GdtPtr32 *p);extern void *orion_exception_stub_table[32];
static volatile uint64_t g_ticks;static uint32_t g_pit_hz=100;static uint8_t keyq[64];static volatile uint8_t key_head,key_tail;static uint8_t mouseq[128];static volatile uint8_t mouse_head,mouse_tail;static int g_mouse_available;static int g_mouse_packet_size=3;
void arch_gdt_init(void){struct GdtPtr32 p={(uint16_t)(sizeof(gdt)-1),(uint32_t)(uintptr_t)gdt};orion_load_gdt(&p);}static void idt_gate(int v,void*fn){uint32_t a=(uint32_t)(uintptr_t)fn;idt[v]=(IdtGate32){(uint16_t)a,0x08,0,0x8e,(uint16_t)(a>>16)};}
static void pic_remap(void){outb(PIC1_DATA,0xFF);outb(PIC2_DATA,0xFF);outb(PIC1,0x11);io_wait();outb(PIC2,0x11);io_wait();outb(PIC1_DATA,0x20);io_wait();outb(PIC2_DATA,0x28);io_wait();outb(PIC1_DATA,4);io_wait();outb(PIC2_DATA,2);io_wait();outb(PIC1_DATA,1);io_wait();outb(PIC2_DATA,1);io_wait();outb(PIC1_DATA,0xF8);outb(PIC2_DATA,0xEF);}
static void pit_program(uint32_t hz){if(hz<19)hz=19;if(hz>1000)hz=1000;uint32_t divisor=PIT_BASE_HZ/hz;if(!divisor)divisor=1;if(divisor>65535)divisor=65535;g_pit_hz=PIT_BASE_HZ/divisor;outb(PIT_CMD,0x36);outb(PIT_CH0,(uint8_t)divisor);outb(PIT_CH0,(uint8_t)(divisor>>8));}
__attribute__((interrupt)) static void pit_isr(struct I686IntFrame*f){(void)f;g_ticks++;outb(PIC1,0x20);}__attribute__((interrupt)) static void keyboard_isr(struct I686IntFrame*f){(void)f;uint8_t s=inb(0x60),n=(uint8_t)(key_head+1)&63;if(n!=key_tail){keyq[key_head]=s;key_head=n;}outb(PIC1,0x20);}__attribute__((interrupt)) static void mouse_isr(struct I686IntFrame*f){(void)f;uint8_t b=inb(0x60),n=(uint8_t)(mouse_head+1)&127;if(n!=mouse_tail){mouseq[mouse_head]=b;mouse_head=n;}outb(PIC2,0x20);outb(PIC1,0x20);}
static int ps2_wait_write(void){for(unsigned i=0;i<100000;i++)if((inb(0x64)&2)==0)return 1;return 0;}static int ps2_wait_read(void){for(unsigned i=0;i<100000;i++)if(inb(0x64)&1)return 1;return 0;}static int mouse_send(uint8_t v){if(!ps2_wait_write())return 0;outb(0x64,0xD4);if(!ps2_wait_write())return 0;outb(0x60,v);return 1;}static int mouse_write(uint8_t v){if(!mouse_send(v)||!ps2_wait_read())return 0;return inb(0x60)==0xFA;}static int mouse_command_read(uint8_t cmd,uint8_t*out){if(!mouse_send(cmd)||!ps2_wait_read()||inb(0x60)!=0xFA||!ps2_wait_read())return 0;if(out)*out=inb(0x60);else(void)inb(0x60);return 1;}
static void mouse_init(void){if(!ps2_wait_write())return;outb(0x64,0xA8);if(!ps2_wait_write())return;outb(0x64,0x20);if(!ps2_wait_read())return;uint8_t cmd=inb(0x60);cmd|=2;cmd&=(uint8_t)~0x20;if(!ps2_wait_write())return;outb(0x64,0x60);if(!ps2_wait_write())return;outb(0x60,cmd);if(!mouse_write(0xF6))return;if(mouse_write(0xF3)&&mouse_write(200)&&mouse_write(0xF3)&&mouse_write(100)&&mouse_write(0xF3)&&mouse_write(80)){uint8_t id=0;if(mouse_command_read(0xF2,&id)&&(id==3||id==4))g_mouse_packet_size=4;}if(!mouse_write(0xF4))return;g_mouse_available=1;}
void interrupts_init(uint32_t pit_hz){for(int i=0;i<256;i++)idt[i]=(IdtGate32){0};for(int i=0;i<32;i++)idt_gate(i,orion_exception_stub_table[i]);idt_gate(32,(void*)pit_isr);idt_gate(33,(void*)keyboard_isr);idt_gate(44,(void*)mouse_isr);struct IdtPtr32 p={(uint16_t)(sizeof(idt)-1),(uint32_t)(uintptr_t)idt};__asm__ volatile("lidt %0"::"m"(p));pic_remap();pit_program(pit_hz);mouse_init();}
uint64_t timer_ticks(void){return g_ticks;}uint32_t timer_frequency(void){return g_pit_hz;}int keyboard_pop_scancode(uint8_t*s){if(key_tail==key_head)return 0;if(s)*s=keyq[key_tail];key_tail=(uint8_t)(key_tail+1)&63;return 1;}int mouse_pop_byte(uint8_t*b){if(mouse_tail==mouse_head)return 0;if(b)*b=mouseq[mouse_tail];mouse_tail=(uint8_t)(mouse_tail+1)&127;return 1;}int mouse_available(void){return g_mouse_available;}int mouse_packet_size(void){return g_mouse_packet_size;}
static const char*exname(uint32_t v){switch(v){case 0:return "DIVIDE ERROR";case 6:return "INVALID OPCODE";case 8:return "DOUBLE FAULT";case 13:return "GENERAL PROTECTION";case 14:return "PAGE FAULT";default:return "CPU EXCEPTION";}}
__attribute__((noreturn)) void orion_exception_panic32(uint32_t v,uint32_t e,uint32_t ip,uint32_t cs,uint32_t fl){(void)cs;(void)fl;__asm__ volatile("cli");serial_write("\r\nUN_Orion i686 PANIC: ");serial_write(exname(v));serial_write(" vector=");serial_write_hex64(v);serial_write(" error=");serial_write_hex64(e);serial_write(" eip=");serial_write_hex64(ip);serial_write("\r\n");if(gfx_ready()){gfx_rect(0,0,gfx_width(),gfx_height(),0x13090D);gfx_display_text(42,76,"PANIC",0xFFE7EC);gfx_text(44,150,exname(v),0xFF8FA3,1);gfx_text(44,214,"i686 / Legacy BIOS",0xB77A89,1);}for(;;)__asm__ volatile("hlt");}
__attribute__((noreturn)) void orion_exception_panic(uint64_t v,uint64_t e,uint64_t ip,uint64_t cs,uint64_t fl){orion_exception_panic32((uint32_t)v,(uint32_t)e,(uint32_t)ip,(uint32_t)cs,(uint32_t)fl);}
