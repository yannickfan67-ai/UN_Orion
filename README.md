# UN_Orion

UN_Orion is a from-scratch x86_64 hobby operating system. It boots through UEFI, leaves firmware services, installs its own basic CPU tables/interrupt path, and draws its own UI directly into the GOP framebuffer.

## Current status — v0.0.2

- x86_64 ELF kernel loaded by a GNU-EFI UEFI loader
- GOP framebuffer UI with RGB/BGR handling
- Traf Typeface v2.1 (Version 2.100) raster data embedded into the kernel build
- Own GDT
- Own IDT and remapped legacy PIC
- PS/2 keyboard IRQ1 input
- Interactive Orion Console
- Commands: `help`, `clear`, `info`, `mem`, `reboot`, `halt`
- COM1 serial diagnostics
- UEFI memory-map handoff and usable-memory display
- Pure-Python FAT16 image builder (`tools/mkfat.py`), so image creation no longer needs mtools/dosfstools
- QEMU/OVMF smoke testing in GitHub Actions

The v2.1 font raster dataset was generated from `TrafTypeface-Regular.ttf` Version 2.100. Source TTF SHA-256:

`7320fdf88753792398172fbbc3432ef1da46d75f0bdcbc982130b162468ace5f`

## Toolchain

On Debian/Ubuntu:

```bash
sudo apt install clang lld llvm make qemu-system-x86 ovmf gnu-efi
```

Python 3 is also required for the self-contained font-data/header generator and FAT16 image builder.

## Build

```bash
make
```

The boot image is generated at `build/orion.img`.

## Run

```bash
make run
```

## Smoke test

```bash
make smoke
```

The smoke test boots the image under OVMF/QEMU and requires the kernel-alive serial marker.

## Next milestones

The current IRQ/console layer is intentionally small. The next kernel milestones are exception handlers, a physical page allocator, paging ownership, a timer/clock source, a filesystem layer, and eventually a userspace/process model.
