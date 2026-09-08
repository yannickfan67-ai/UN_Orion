#include <stdint.h>
#include "io.h"
#include "pci.h"

static uint32_t addr(uint8_t b,uint8_t d,uint8_t f,uint8_t o){
    return 0x80000000u|((uint32_t)b<<16)|((uint32_t)d<<11)|((uint32_t)f<<8)|(o&0xfcu);
}
uint32_t pci_read32(uint8_t b,uint8_t d,uint8_t f,uint8_t o){outl(0xcf8,addr(b,d,f,o));return inl(0xcfc);}
void pci_write32(uint8_t b,uint8_t d,uint8_t f,uint8_t o,uint32_t v){outl(0xcf8,addr(b,d,f,o));outl(0xcfc,v);}
uint16_t pci_read16(uint8_t b,uint8_t d,uint8_t f,uint8_t o){uint32_t v=pci_read32(b,d,f,o);return (uint16_t)(v>>((o&2u)*8u));}
void pci_write16(uint8_t b,uint8_t d,uint8_t f,uint8_t o,uint16_t v){uint8_t a=(uint8_t)(o&0xfcu);uint32_t x=pci_read32(b,d,f,a);unsigned s=(o&2u)*8u;x=(x&~(0xffffu<<s))|((uint32_t)v<<s);pci_write32(b,d,f,a,x);}
int pci_find(uint16_t vendor,uint16_t device,PciDevice *out){
    for(unsigned b=0;b<256;b++)for(unsigned d=0;d<32;d++)for(unsigned f=0;f<8;f++){
        uint32_t id=pci_read32((uint8_t)b,(uint8_t)d,(uint8_t)f,0);
        if((id&0xffffu)==0xffffu){if(f==0)break;continue;}
        if((uint16_t)id==vendor&&(uint16_t)(id>>16)==device){
            uint32_t cc=pci_read32((uint8_t)b,(uint8_t)d,(uint8_t)f,8);
            if(out){out->bus=(uint8_t)b;out->dev=(uint8_t)d;out->fn=(uint8_t)f;out->vendor=vendor;out->device=device;out->revision=(uint8_t)cc;out->prog_if=(uint8_t)(cc>>8);out->subclass=(uint8_t)(cc>>16);out->class_code=(uint8_t)(cc>>24);}return 1;
        }
        if(f==0){uint32_t h=pci_read32((uint8_t)b,(uint8_t)d,0,0x0c);if(!((h>>16)&0x80))break;}
    }
    return 0;
}
uint32_t pci_bar32(const PciDevice *d,unsigned bar){if(!d||bar>5)return 0;return pci_read32(d->bus,d->dev,d->fn,(uint8_t)(0x10+bar*4));}
uint64_t pci_bar64(const PciDevice *d,unsigned bar){uint32_t lo=pci_bar32(d,bar);uint64_t base=(uint64_t)(lo&~0xfull);if(((lo>>1)&3u)==2u&&bar<5)base|=(uint64_t)pci_bar32(d,bar+1)<<32;return base;}
void pci_enable(const PciDevice *d,uint16_t bits){if(!d)return;uint16_t c=pci_read16(d->bus,d->dev,d->fn,0x04);pci_write16(d->bus,d->dev,d->fn,0x04,(uint16_t)(c|bits));}
