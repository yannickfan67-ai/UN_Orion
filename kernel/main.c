#include <stdint.h>
#include "bootinfo.h"
#include "serial.h"
#include "graphics.h"
#include "interrupts.h"
#include "pmm.h"
#include "desktop.h"
#include "net.h"

__attribute__((noreturn)) void kernel_main(OrionBootInfo *bi){
    __asm__ volatile("cli");
    serial_init();
    gfx_init(bi);
    serial_write("UN_Orion kernel 0.0.5 alive\r\n");
    arch_gdt_init();
    serial_write("GDT ready\r\n");
    pmm_init(bi);
    serial_write("PMM ready\r\n");
    interrupts_init(100);
    serial_write("IDT/PIC/PIT/keyboard ready\r\n");
    serial_write(mouse_available()?"PS/2 mouse IRQ12 ready\r\n":"PS/2 mouse unavailable\r\n");
    net_init();
    serial_write(net_ready()?"Network stack ready\r\n":"Network adapter unavailable\r\n");
    if(gfx_ready()){
        desktop_init(bi);
        serial_write("Orion desktop ready\r\n");
    }
    __asm__ volatile("sti");
    for(;;){
        uint8_t b;
        while(keyboard_pop_scancode(&b))desktop_key_scancode(b);
        while(mouse_pop_byte(&b))desktop_mouse_byte(b);
        net_poll();
        desktop_tick();
        __asm__ volatile("sti; hlt");
    }
}
