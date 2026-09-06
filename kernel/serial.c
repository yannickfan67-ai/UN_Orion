#include <stdint.h>
#include "io.h"
#include "serial.h"
#define COM1 0x3F8
void serial_init(void){
    outb(COM1+1,0x00); outb(COM1+3,0x80); outb(COM1+0,0x03); outb(COM1+1,0x00);
    outb(COM1+3,0x03); outb(COM1+2,0xC7); outb(COM1+4,0x0B);
}
void serial_putc(char c){ while((inb(COM1+5)&0x20)==0){} outb(COM1,(uint8_t)c); }
void serial_write(const char*s){ while(s&&*s) serial_putc(*s++); }
void serial_write_hex64(uint64_t v){
    static const char hex[]="0123456789ABCDEF";
    serial_write("0x");
    for(int i=15;i>=0;i--) serial_putc(hex[(v>>(i*4))&0xF]);
}
