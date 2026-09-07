# 86Box newest practical UN_Orion profile

This profile is the late-PC compatibility baseline actually exercised with **86Box 6.0 build 9001** and an external 86Box ROM set. ROM binaries and the 86Box AppImage are test dependencies and are **not** checked into UN_Orion.

## Tested machine

- machine: Samsung CAIRO-5 / late Socket 370 platform (`cairo5`)
- firmware: PhoenixBIOS 4.0 Release 6.0, Samsung 2001-era BIOS
- CPU configuration: VIA/Centaur Cyrix III (Samuel), 600 MHz (`c3_samuel`)
- CPUID vendor observed by UN_Orion: `CentaurHauls`
- RAM: 128 MiB
- graphics: 3dfx Voodoo3 3500 AGP
- storage: internal IDE / ATA PIO
- input: AT/PS/2
- network: NE2000 ISA, I/O 0x300, IRQ 3
- audio: Sound Blaster 16

The old Samsung/Phoenix BIOS displays the configured Samuel CPU as **Intel Celeron 600 MHz**. UN_Orion does not trust that string: its i686 bootstrap executes CPUID directly and observes `CentaurHauls`.

## What was actually verified

A hardware probe on this profile completed all seven low-level checks:

1. VGA BIOS mode access
2. UART register communication
3. 8042 controller self-test
4. NE2000 reset + remote-DMA PROM/MAC read
5. ATA `IDENTIFY DEVICE`
6. ATA LBA28 write + readback on a disposable test HDD
7. Sound Blaster DSP reset/acknowledge

The UN_Orion i686 Legacy-BIOS bootstrap then entered 32-bit protected mode, executed CPUID, and in the smoke build wrote `ORIONI686-PMODE-BOOT-OK` to LBA 11 of a **dedicated disposable HDD**. The host read the same signature back. Normal released i686 images do not perform this write.

## Scope

“Newest practical profile” means the newest late-1990s/early-2000s configuration selected and boot-tested from the current 86Box build/ROM set for Orion compatibility work. It is not a claim that 86Box emulates current PC platforms.

Use `compat/86box/cairo5-c3-600.cfg` as the reproducible starting point. Paths may be adjusted locally; do not commit ROMs or user disk images.
