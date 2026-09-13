#include <stdint.h>
#include "bootinfo.h"
#include "serial.h"
#include "graphics.h"
#include "interrupts.h"
#include "pmm.h"
#include "desktop.h"
#include "net.h"
#include "dhcp.h"
#include "version.h"

__attribute__((noreturn)) void kernel_main(OrionBootInfo *bi){
    __asm__ volatile("cli");
    serial_init();
    gfx_init(bi);
    serial_write(ORION_VERSION_STRING " alive\r\n");
    arch_gdt_init();
    serial_write("GDT ready\r\n");
    pmm_init(bi);
    serial_write("PMM ready\r\n");
    interrupts_init(100);
    serial_write("IDT/PIC/PIT/keyboard ready\r\n");
    serial_write(mouse_available()?"PS/2 mouse IRQ12 ready\r\n":"PS/2 mouse unavailable\r\n");
    int network_up=net_init();
    if(network_up){
        uint8_t mac[6];
        OrionDhcpLease lease;
        net_get_mac(mac);
        if(dhcp_acquire(mac,1800,&lease)){
            net_configure(lease.ip,lease.netmask,lease.gateway,lease.dns);
            serial_write("DHCPv4 lease acquired\r\n");
        }else{
            serial_write("DHCPv4 unavailable; static IPv4 fallback active\r\n");
        }
    }
    serial_write(net_ready()?"Network stack ready\r\n":"Network adapter unavailable\r\n");
    if(gfx_ready()){
        serial_write(bi&&bi->pixel_format==ORION_PIXEL_FORMAT_RGB565?"Framebuffer RGB565 ready\r\n":"Framebuffer 32-bit ready\r\n");
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
