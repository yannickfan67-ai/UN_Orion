#ifndef ORION_IMAGE_ABI_H
#define ORION_IMAGE_ABI_H

#define ORION_IMAGE_SCHEMA_VERSION 1u

#define ORION_ARCH_I686    1u
#define ORION_ARCH_X86_64  2u
#define ORION_ARCH_AARCH64 3u
#define ORION_ARCH_RISCV64 4u

#define ORION_FIRMWARE_BIOS 1u
#define ORION_FIRMWARE_UEFI 2u

#define ORION_KERNEL_ELF32 1u
#define ORION_KERNEL_ELF64 2u

/* BootInfo ABI is versioned independently from image/media layout. */
#define ORION_BOOTINFO_ABI_MAJOR 1u
#define ORION_BOOTINFO_ABI_MINOR 0u

#endif
