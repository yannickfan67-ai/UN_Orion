#ifndef ORION_PCI_H
#define ORION_PCI_H
#include <stdint.h>

typedef struct {
    uint8_t bus, dev, fn;
    uint16_t vendor, device;
    uint8_t revision, prog_if, subclass, class_code;
} PciDevice;

uint32_t pci_read32(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off);
void pci_write32(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off,uint32_t value);
uint16_t pci_read16(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off);
void pci_write16(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off,uint16_t value);
int pci_find(uint16_t vendor,uint16_t device,PciDevice *out);
uint32_t pci_bar32(const PciDevice *d,unsigned bar);
uint64_t pci_bar64(const PciDevice *d,unsigned bar);
void pci_enable(const PciDevice *d,uint16_t command_bits);
#endif
