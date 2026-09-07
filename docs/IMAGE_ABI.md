# Orion Image ABI v1

The Image ABI describes **what a boot artifact requires before it is launched**. It is deliberately separate from the kernel's internal implementation, ODI driver ABI, ORX application ABI and Cygnus VM configuration format.

Every released boot medium should ship with a JSON manifest using schema `org.un.orion.image-manifest`, schema version 1.

## Stable architecture IDs

| ID | Architecture | Status |
|---:|---|---|
| 1 | i686 | experimental Legacy-BIOS compatibility bootstrap implemented |
| 2 | x86_64 | implemented full desktop target |
| 3 | AArch64 | reserved / future ARM64 port |
| 4 | RISC-V64 | reserved / future port |

These IDs are shared conceptually with ODI runtime architecture IDs, but package formats that already have a published registry (such as ODRV v1) keep their own existing numeric values for binary compatibility.

## Firmware IDs

- `1` BIOS/legacy firmware
- `2` UEFI

UN_Orion currently has two independently declared boot contracts: x86_64 + UEFI for the full desktop system, and an experimental i686 + Legacy BIOS bootstrap for old-PC/86Box compatibility work.

## Manifest contract

Consumers such as installers, deployment tools, UN_Cygnus, v86/86Box launch helpers or physical-media writers should inspect:

- schema version
- architecture name/ID and bitness
- firmware requirement
- kernel payload format
- BootInfo ABI when present
- minimum RAM
- required/optional input, framebuffer and network device contracts
- media SHA-256

A consumer must reject an unknown schema major or an architecture/firmware combination it cannot satisfy. Unknown additive fields are ignored.

## Compatibility layers

The intended boundaries are:

`boot media -> Image ABI -> loader -> BootInfo ABI -> kernel -> ODI services -> ORX apps`

The i686 bootstrap is intentionally earlier in this stack: it proves BIOS loading, A20, GDT/protected mode, CPUID and legacy hardware access while the full desktop/services are ported behind stable interfaces.

UN_Vela and Aster Engine sit above platform callbacks rather than depending on a specific CPU architecture. UN_Cygnus consumes the same image requirements before selecting an emulation/hardware-virtualization backend.

## Media types

v1 standard media types:
- `disk-image`
- `installer-iso`

A `disk-image` may be an EFI/FAT system image or a BIOS-bootable legacy image; the manifest firmware and architecture fields disambiguate it.

## x86_64 / UEFI 0.0.5 contract

- architecture: x86_64
- firmware: UEFI x64
- kernel: ELF64
- minimum test RAM: 256 MiB
- framebuffer: UEFI GOP RGB/BGR
- keyboard: PS/2 baseline
- mouse: PS/2 optional
- network: RTL8139 optional
- full desktop, UN_Vela/Aster and early IPv4/HTTP stack

## i686 / Legacy BIOS experimental contract

- architecture: i686
- firmware: Legacy BIOS
- payload: 16-bit boot sector -> 32-bit protected-mode flat bootstrap
- minimum declared RAM: 4 MiB
- display: VGA text mode
- CPU identification: CPUID vendor string
- ODI compatibility target: ABI 1.1 legacy-PC path
- normal image performs **no automatic disk writes**
- dedicated smoke builds may write a signature to a disposable ATA test disk for end-to-end verification

This target does **not** yet claim feature parity with the x86_64 desktop. It exists so legacy-PC drivers and architecture-neutral subsystems can be moved over without faking support.

See `docs/86BOX_NEWEST_PROFILE.md` for the newest practical 86Box configuration actually boot-tested with this target.
