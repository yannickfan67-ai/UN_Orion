#include <stdint.h>
#include <stddef.h>
#include "io.h"
#include "pci.h"
#include "netdev_internal.h"
#define RXBUF_SIZE (8192+16+1500)
static uint16_t base,cur;static int txcur;static int up;
static uint8_t rxbuf[RXBUF_SIZE] __attribute__((aligned(256)));
static uint8_t txbuf[4][2048] __attribute__((aligned(16)));
static void cp(void*d,const void*s,size_t n){uint8_t*a=d;const uint8_t*b=s;while(n--)*a++=*b++;}
static void zero(void*d,size_t n){uint8_t*p=d;while(n--)*p++=0;}
static int init(uint8_t mac[6]){PciDevice d;if(!pci_find(0x10ec,0x8139,&d))return 0;uint32_t bar=pci_bar32(&d,0);if(!(bar&1))return 0;base=(uint16_t)(bar&~3u);pci_enable(&d,0x0005);outb((uint16_t)(base+0x37),0x10);for(unsigned i=0;i<1000000&&(inb((uint16_t)(base+0x37))&0x10);i++)__asm__ volatile("pause");for(int i=0;i<6;i++)mac[i]=inb((uint16_t)(base+i));zero(rxbuf,sizeof(rxbuf));outl((uint16_t)(base+0x30),(uint32_t)(uintptr_t)rxbuf);outw((uint16_t)(base+0x3c),0);outl((uint16_t)(base+0x44),0x8a);outl((uint16_t)(base+0x40),0x03000000);outb((uint16_t)(base+0x37),0x0c);cur=0;txcur=0;up=1;return 1;}
static int tx(const void *data,size_t len){if(!up||len<14||len>1792)return 0;int s=txcur++&3;cp(txbuf[s],data,len);outl((uint16_t)(base+0x20+s*4),(uint32_t)(uintptr_t)txbuf[s]);outl((uint16_t)(base+0x10+s*4),(uint32_t)len);return 1;}
static void poll(netdev_rx_fn rx){if(!up)return;int guard=32;while(guard--&&!(inb((uint16_t)(base+0x37))&1)){uint16_t off=(uint16_t)(cur%8192),st=*(volatile uint16_t*)(rxbuf+off),len=*(volatile uint16_t*)(rxbuf+off+2);if(!(st&1)||len<4||len>1600){cur=0;outw((uint16_t)(base+0x38),0xfff0);break;}uint8_t pkt[1600];size_t n=len-4;for(size_t i=0;i<n;i++)pkt[i]=rxbuf[(off+4+i)%8192];rx(pkt,n);cur=(uint16_t)((cur+len+4+3)&~3u);outw((uint16_t)(base+0x38),(uint16_t)(cur-16));}}
const NetDriver rtl8139_driver={"RTL8139",0,init,tx,poll};
