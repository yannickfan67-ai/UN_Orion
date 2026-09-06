# UN_Orion

UN_Orion is a from-scratch x86_64 hobby operating system project.

## v0.0.1 goals

- UEFI boot
- Load a 64-bit ELF kernel
- Pass framebuffer and memory-map information to the kernel
- Draw directly to the GOP framebuffer
- Keep the codebase small enough to extend with GDT/IDT, paging, allocator, input, and filesystems

## Toolchain

Recommended on Debian/Ubuntu:

```bash
sudo apt install clang lld llvm make qemu-system-x86 ovmf gnu-efi mtools dosfstools
```

The current bootstrap uses GNU-EFI for the loader and Clang/LLD for the kernel.

## Build

```bash
make
```

## Run

```bash
make run
```

The image is generated at `build/orion.img`.

## Status

v0.0.1 bootstrap: early UEFI + framebuffer bring-up. Expect rapid changes while memory management, interrupts, input, and the Orion shell are added.
