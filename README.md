# UN_Orion

UN_Orion is a from-scratch x86_64 hobby operating system. It boots through UEFI, leaves firmware services, installs its own CPU tables and interrupt path, and draws its own interface directly into the GOP framebuffer.

## Current development status — v0.0.3

### Boot and display
- x86_64 ELF kernel loaded by a GNU-EFI UEFI loader
- GOP framebuffer UI with RGB/BGR handling
- Traf Typeface v2.1 / Version 2.100 embedded as kernel bitmap data
- self-contained Python FAT16 image builder (`tools/mkfat.py`)

### CPU and interrupts
- own GDT
- own IDT
- CPU exception stubs for vectors 0–31
- framebuffer + COM1 panic screen with vector/error/RIP diagnostics
- remapped 8259 PIC
- PIT IRQ0 clock at ~100 Hz
- PS/2 keyboard IRQ1 input

### Memory
- UEFI memory-map handoff
- early 4 KiB physical page allocator over conventional memory
- low-memory and kernel/framebuffer/boot-info reservations
- PMM total/free/used statistics

### Console
Interactive Orion Console commands:

`help`, `clear`, `info`, `cpu`, `mem`, `pmm`, `alloc`, `uptime`, `reboot`, `halt`, `fault`

`fault` deliberately executes an invalid opcode to test the CPU exception/panic path.

### Diagnostics
- COM1 serial boot diagnostics
- CPU vendor/brand via CPUID
- live uptime display driven by PIT ticks
- QEMU/OVMF smoke tests in GitHub Actions

## Releases

The first packaged developer release is `v0.0.2`. Current `main` is ahead of that release while v0.0.3 is developed and tested.

## Toolchain

On Debian/Ubuntu:

```bash
sudo apt install clang lld llvm make qemu-system-x86 ovmf gnu-efi
```

Python 3 is required for the font-data generator and FAT16 image builder.

## Build

```bash
make
```

The image is generated at `build/orion.img`.

## Run

```bash
make run
```

## Smoke test

```bash
make smoke
```

## Next kernel work

The next major steps are taking ownership of x86_64 page tables, adding a reusable page free-list/bitmap, moving from the legacy PIT/PIC path toward APIC/HPET where available, reading files from the boot filesystem inside the kernel, and defining a syscall/userspace process boundary.
