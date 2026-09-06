# UN_Orion

UN_Orion is a from-scratch x86_64 operating system project. It boots through UEFI and runs its own kernel, interrupts, memory allocator, graphics stack, input path, and desktop environment.

## Current development status — v0.0.4 Desktop Preview

The default interface is now a desktop rather than a hardware/status dashboard.

### Desktop

- Traf Typeface v2.1 throughout the UI
- wallpaper and bottom taskbar
- launcher / Start menu
- real PS/2 mouse support through IRQ12
- software mouse cursor
- draggable/focusable/minimizable/closable windows
- taskbar app switching
- desktop application shortcuts

### Native applications

- **Terminal** — interactive Orion shell
- **Files** — desktop-style file/application browser shell
- **Notes** — editable text document in RAM
- **Paint** — mouse-driven drawing canvas with Clear action
- **About** — kernel/memory/input information, moved out of the main desktop

### Kernel foundation

- x86_64 UEFI ELF loader
- GOP framebuffer graphics
- own GDT and IDT
- 8259 PIC remap
- PIT IRQ0 at ~100 Hz
- PS/2 keyboard IRQ1
- PS/2 mouse IRQ12
- physical page allocator using the UEFI memory map
- CPU exception vectors 0–31 with graphical panic screen
- COM1 serial diagnostics
- pure-Python FAT16 image builder

## Build

On Debian/Ubuntu:

```bash
sudo apt install clang lld llvm make qemu-system-x86 ovmf gnu-efi
make
make run
```

The boot image is written to `build/orion.img`.

## Terminal commands

`help`, `clear`, `info`, `mem`, `alloc`, `uptime`, `desktop`

## Direction

The desktop is now the primary UI. Upcoming work should make the applications deeper: persistent files, real disk/filesystem access, application/process separation, richer widgets, and eventually userspace instead of moving back toward a diagnostics-first interface.
