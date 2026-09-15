# UN_Orion roadmap

This document tracks subsystem work rather than marketing milestones.

## Storage and persistence

- generic block-device ABI with checked LBA bounds and optional write support is implemented
- initial read-only FAT16 mount/geometry validation is implemented for 512-byte logical sectors, including bounded root-directory sector reads, short 8.3 root entry lookup, checked data-cluster reads and FAT chain stepping
- connect an ATA/IDE PIO reference driver to the block-device ABI first, then AHCI and NVMe
- extend FAT16 from checked cluster stepping to bounded whole-file reads, then guarded write support
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

- initial DHCPv4 DORA boot client is implemented with static fallback and packet-level regression coverage
- add DHCP lease renewal/rebinding, configurable DNS and persistent network settings
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

- early PMM now allocates conventional pages and supports bounded single-page recycling; replace it with a bitmap/buddy allocator with unrestricted free support
- kernel-owned page tables and virtual memory manager
- APIC/IOAPIC and MSI/MSI-X after legacy PIC bring-up remains stable
- process/thread scheduler, syscall ABI and userspace
- VFS, handles, capabilities and security boundaries

## Desktop/system

- Settings persistence
- clipboard and notifications
- audio stack
- installer UI only after storage writes are reliable
- future **Cygnus Manager** frontend for creating and controlling UN_Cygnus VMs

## Maintenance

- keep `Makefile`, `include/version.h`, README media names and CI artifact names version-synchronized
- use the version-agnostic tag release workflow instead of adding new hard-coded release workflows
- keep x86_64 UEFI, i686 floppy/HDD, Cirrus RGB565, DHCP, installer and HTTPS smoke coverage green
- keep block-device and FAT16 host/freestanding regression coverage green while storage is brought online
- prefer host-side regression tests for pure subsystems before extending full-system QEMU coverage
