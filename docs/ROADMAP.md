# UN_Orion roadmap

This document tracks subsystem work rather than marketing milestones.

## Storage and persistence

- split PCI enumeration, block devices and filesystems out of monolithic modules
- ATA/IDE PIO reference block driver first, then AHCI and NVMe
- FAT16 read/write for the existing boot media
- persistent Files and Notes
- real install-to-disk path from the UEFI optical media

## Applications

- load `.orx` applications from disk through Orion App ABI 1
- resource/icon sections and app associations
- application permissions and process ownership
- move desktop applications out of kernel mode after userspace exists

## Drivers

- consume ODI device/lifecycle interfaces
- external `.odrv` loader with ABI/version checks
- interrupt, DMA and MMIO services exposed through a kernel driver API
- QEMU EDU/fw_cfg reference drivers, then VirtIO transport and real-device drivers

## Networking

- DHCPv4 and configurable DNS
- general socket API instead of one synchronous TCP transaction
- TCP retransmission/window handling
- TLS/HTTPS foundation
- virtual-switch integration with UN_Cygnus later

## Browser stack

- keep UN_Vela and Aster as separately maintained components
- remove remaining legacy HTTP markup normalization so Aster alone owns HTML parsing
- URL/link navigation, history, downloads and forms
- CSS/style engine and images in Aster
- preserve Orion-Browser as a low-memory/recovery path

## Kernel architecture

- kernel-owned page tables and virtual memory manager
- bitmap/buddy physical allocator with free support
- APIC/IOAPIC and MSI/MSI-X after legacy PIC bring-up remains stable
- process/thread scheduler, syscall ABI and userspace
- VFS, handles, capabilities and security boundaries

## Desktop/system

- Settings persistence
- clipboard and notifications
- audio stack
- installer UI only after storage writes are reliable
- future **Cygnus Manager** frontend for creating and controlling UN_Cygnus VMs
