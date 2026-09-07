#!/usr/bin/env python3
"""Build the UN_Orion i686 Legacy-BIOS 1.44 MiB boot image."""
import argparse
from pathlib import Path

FLOPPY_SIZE = 1440 * 1024
STAGE2_SECTORS = 16


def main():
    p = argparse.ArgumentParser()
    p.add_argument("boot")
    p.add_argument("stage2")
    p.add_argument("output")
    a = p.parse_args()

    boot = Path(a.boot).read_bytes()
    stage2 = Path(a.stage2).read_bytes()
    if len(boot) != 512 or boot[510:512] != b"\x55\xaa":
        raise SystemExit("legacy boot sector must be exactly 512 bytes with 55aa signature")
    limit = STAGE2_SECTORS * 512
    if len(stage2) > limit:
        raise SystemExit(f"stage2 too large: {len(stage2)} > {limit}")

    image = bytearray(FLOPPY_SIZE)
    image[:512] = boot
    image[512:512 + len(stage2)] = stage2
    Path(a.output).write_bytes(image)
    print(f"wrote {a.output}: {len(image)} bytes, stage2={len(stage2)} bytes")


if __name__ == "__main__":
    main()
