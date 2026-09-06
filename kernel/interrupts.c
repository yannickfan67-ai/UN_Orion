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

typedef struct __attribute__((packed)){
    uint16_t off0; uint16_t sel; uint8_t ist; uint8_t attr;
    uint16_t off1; uint32_t off2; uint32_t zero;
} IdtGate;
struct __attribute__((packed)) IdtPtr{uint16_t limit;uint64_t base;};
struct __attribute__((packed)) GdtPtr{uint16_t limit;uint64_t base;};
struct IntFrame{uint64_t rip,cs,rflags,rsp,ss;};

static IdtGate idt[256];
static uint64_t gdt[3]={0,0x00AF9A000000FFFFULL,0x00CF92000000FFFFULL};
extern void orion_load_gdt(struct GdtPtr *p);
extern void *orion_exception_stub_table[32];
static volatile uint64_t g_ticks;
static uint32_t g_pit_hz=100;
static uint8_t keyq[64];
static volatile uint8_t key_head,key_tail;

void arch_gdt_init(void){struct GdtPtr p={sizeof(gdt)-1,(uint64_t)(uintptr_t)gdt};orion_load_gdt(&p);}

static void idt_gate(int v,void *fn){
    uint64_t a=(uint64_t)(uintptr_t)fn;
    idt[v]=(IdtGate){(uint16_t)(a&0xffff),0x08,0,0x8e,(uint16_t)((a>>16)&0xffff),(uint32_t)(a>>32),0};
}
static void pic_remap(void){
    outb(PIC1_DATA,0xFF); outb(PIC2_DATA,0xFF);
    outb(PIC1,0x11);io_wait(); outb(PIC2,0x11);io_wait();
    outb(PIC1_DATA,0x20);io_wait(); outb(PIC2_DATA,0x28);io_wait();
    outb(PIC1_DATA,4);io_wait(); outb(PIC2_DATA,2);io_wait();
    outb(PIC1_DATA,1);io_wait(); outb(PIC2_DATA,1);io_wait();
    outb(PIC1_DATA,0xFC); outb(PIC2_DATA,0xFF);
}
static void pit_program(uint32_t hz){
    if(hz<19)hz=19; if(hz>1000)hz=1000;
    uint32_t divisor=PIT_BASE_HZ/hz; if(divisor==0)divisor=1;if(divisor>65535)divisor=65535;
    g_pit_hz=PIT_BASE_HZ/divisor;
    outb(PIT_CMD,0x36); outb(PIT_CH0,(uint8_t)(divisor&0xff)); outb(PIT_CH0,(uint8_t)(divisor>>8));
}
__attribute__((interrupt)) static void pit_isr(struct IntFrame *f){(void)f;g_ticks++;outb(PIC1,0x20);}
__attribute__((interrupt)) static void keyboard_isr(struct IntFrame *f){
    (void)f; uint8_t s=inb(0x60); uint8_t next=(uint8_t)(key_head+1)&63;
    if(next!=key_tail){keyq[key_head]=s;key_head=next;} outb(PIC1,0x20);
}
void interrupts_init(uint32_t pit_hz){
    for(int i=0;i<256;i++)idt[i]=(IdtGate){0};
    for(int i=0;i<32;i++)idt_gate(i,orion_exception_stub_table[i]);
    idt_gate(32,(void*)pit_isr); idt_gate(33,(void*)keyboard_isr);
    struct IdtPtr p={sizeof(idt)-1,(uint64_t)(uintptr_t)idt}; __asm__ volatile("lidt %0"::"m"(p));
    pic_remap(); pit_program(pit_hz);
}
uint64_t timer_ticks(void){return g_ticks;}
uint32_t timer_frequency(void){return g_pit_hz;}
int keyboard_pop_scancode(uint8_t *scan){
    if(key_tail==key_head)return 0; if(scan)*scan=keyq[key_tail]; key_tail=(uint8_t)(key_tail+1)&63; return 1;
}
static const char *exception_name(uint64_t v){
    switch(v){
        case 0:return "DIVIDE ERROR"; case 1:return "DEBUG"; case 2:return "NON-MASKABLE INTERRUPT";
        case 3:return "BREAKPOINT"; case 4:return "OVERFLOW"; case 5:return "BOUND RANGE";
        case 6:return "INVALID OPCODE"; case 7:return "DEVICE NOT AVAILABLE"; case 8:return "DOUBLE FAULT";
        case 9:return "COPROCESSOR SEGMENT"; case 10:return "INVALID TSS"; case 11:return "SEGMENT NOT PRESENT";
        case 12:return "STACK FAULT"; case 13:return "GENERAL PROTECTION"; case 14:return "PAGE FAULT";
        case 16:return "X87 FLOATING POINT"; case 17:return "ALIGNMENT CHECK"; case 18:return "MACHINE CHECK";
        case 19:return "SIMD FLOATING POINT"; case 20:return "VIRTUALIZATION"; case 21:return "CONTROL PROTECTION";
        case 29:return "VMM COMMUNICATION"; case 30:return "SECURITY EXCEPTION"; default:return "CPU EXCEPTION";
    }
}
static void hexstr(uint64_t v,char out[19]){
    static const char h[]="0123456789ABCDEF";out[0]='0';out[1]='x';for(int i=0;i<16;i++)out[2+i]=h[(v>>(60-i*4))&15];out[18]=0;
}
__attribute__((noreturn)) void orion_exception_panic(uint64_t vector,uint64_t error,uint64_t rip,uint64_t cs,uint64_t rflags){
    __asm__ volatile("cli");
    serial_write("\r\nUN_Orion PANIC: ");serial_write(exception_name(vector));serial_write(" vector=");serial_write_hex64(vector);
    serial_write(" error=");serial_write_hex64(error);serial_write(" rip=");serial_write_hex64(rip);serial_write("\r\n");
    uint64_t cr2=0;if(vector==14)__asm__ volatile("mov %%cr2,%0":"=r"(cr2));
    if(gfx_ready()){
        char hv[19];uint32_t W=gfx_width(),H=gfx_height();
        gfx_rect(0,0,W,H,0x13090D);gfx_rect(0,0,7,H,0xFF5C77);gfx_rect(0,0,W,58,0x1D0E14);
        gfx_display_text(42,76,"PANIC",0xFFE7EC);gfx_text(44,150,exception_name(vector),0xFF8FA3,1);
        gfx_line_h(44,188,W>88?W-88:0,0x5C2633);
        gfx_text(44,214,"VECTOR",0xA86878,1);hexstr(vector,hv);gfx_text(190,214,hv,0xFFE7EC,1);
        gfx_text(44,250,"ERROR",0xA86878,1);hexstr(error,hv);gfx_text(190,250,hv,0xFFE7EC,1);
        gfx_text(44,286,"RIP",0xA86878,1);hexstr(rip,hv);gfx_text(190,286,hv,0xFFE7EC,1);
        gfx_text(44,322,"CS",0xA86878,1);hexstr(cs,hv);gfx_text(190,322,hv,0xFFE7EC,1);
        gfx_text(44,358,"RFLAGS",0xA86878,1);hexstr(rflags,hv);gfx_text(190,358,hv,0xFFE7EC,1);
        if(vector==14){gfx_text(44,394,"CR2",0xA86878,1);hexstr(cr2,hv);gfx_text(190,394,hv,0xFFE7EC,1);}
        gfx_text(44,H>90?H-76:430,"SYSTEM HALTED / CHECK COM1 FOR DETAILS",0xB77A89,1);
    }
    for(;;)__asm__ volatile("hlt");
}
