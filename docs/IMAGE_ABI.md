# Orion Image ABI v1

The Image ABI describes **what a boot artifact requires before it is launched**. It is deliberately separate from the kernel's internal implementation, ODI driver ABI, ORX application ABI and Cygnus VM configuration format.

Every released boot medium should ship with a JSON manifest using schema `org.un.orion.image-manifest`, schema version 1.

## Stable architecture IDs

| ID | Architecture | Status |
|---:|---|---|
| 1 | i686 | reserved / future legacy-PC port |
| 2 | x86_64 | implemented |
| 3 | AArch64 | reserved / future ARM64 port |
| 4 | RISC-V64 | reserved / future port |

These IDs are shared conceptually with ODI runtime architecture IDs, but package formats that already have a published registry (such as ODRV v1) keep their own existing numeric values for binary compatibility.

## Firmware IDs

- `1` BIOS/legacy firmware
- `2` UEFI

UN_Orion 0.0.5 implements x86_64 + UEFI. A future i686/BIOS image may use the same schema without pretending that the current kernel can boot there today.

## Manifest contract

Consumers such as installers, deployment tools, UN_Cygnus, v86/86Box launch helpers or physical-media writers should inspect:

- schema version
- architecture name/ID and bitness
- firmware requirement
- kernel payload format
- BootInfo ABI
- minimum RAM
- required/optional input, framebuffer and network device contracts
- media SHA-256

A consumer must reject an unknown schema major or an architecture/firmware combination it cannot satisfy. Unknown additive fields are ignored.

## Compatibility layers

The intended boundaries are:

`boot media -> Image ABI -> loader -> BootInfo ABI -> kernel -> ODI services -> ORX apps`

UN_Vela and Aster Engine sit above platform callbacks rather than depending on a specific CPU architecture. UN_Cygnus consumes the same image requirements before selecting an emulation/hardware-virtualization backend.

## Media types

v1 standard media types:
- `disk-image`
- `installer-iso`

Both may describe the same kernel build while having different hashes and boot/install behavior.

## Current 0.0.5 contract

- architecture: x86_64
- firmware: UEFI x64
- kernel: ELF64
- minimum test RAM: 256 MiB
- framebuffer: UEFI GOP RGB/BGR
- keyboard: PS/2 baseline
- mouse: PS/2 optional
- network: RTL8139 optional

This is a declared compatibility contract, not a claim that every emulator or physical machine implementing those labels has already been tested.
