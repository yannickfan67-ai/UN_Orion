#!/usr/bin/env python3
"""Build a full UN_Orion i686 Legacy-BIOS 1.44 MiB system image."""
import argparse, math, struct
from pathlib import Path

FLOPPY_SIZE = 1440 * 1024
STAGE2_SECTORS = 16
HEADER_LBA = 17
KERNEL_LBA = 18
KERNEL_TMP_MAX = 0x80000  # staged at 0x10000; stay below conventional-memory ceiling


def main():
    p=argparse.ArgumentParser()
    p.add_argument("boot")
    p.add_argument("stage2")
    p.add_argument("output")
    p.add_argument("kernel", nargs="?")
    a=p.parse_args()
    boot=Path(a.boot).read_bytes(); stage2=Path(a.stage2).read_bytes()
    if len(boot)!=512 or boot[510:512]!=b"\x55\xaa":
        raise SystemExit("legacy boot sector must be exactly 512 bytes with 55aa signature")
    if len(stage2)>STAGE2_SECTORS*512:
        raise SystemExit(f"stage2 too large: {len(stage2)} > {STAGE2_SECTORS*512}")
    image=bytearray(FLOPPY_SIZE); image[:512]=boot; image[512:512+len(stage2)]=stage2
    if a.kernel:
        kernel=Path(a.kernel).read_bytes()
        if len(kernel)>KERNEL_TMP_MAX:
            raise SystemExit(f"i686 kernel too large for BIOS staging area: {len(kernel)} > {KERNEL_TMP_MAX}")
        sectors=math.ceil(len(kernel)/512)
        if (KERNEL_LBA+sectors)*512>FLOPPY_SIZE:
            raise SystemExit("i686 kernel does not fit floppy image")
        hdr=struct.pack("<4sIHH",b"ORK2",len(kernel),sectors,1)
        image[HEADER_LBA*512:HEADER_LBA*512+len(hdr)]=hdr
        image[KERNEL_LBA*512:KERNEL_LBA*512+len(kernel)]=kernel
        print(f"embedded i686 kernel: {len(kernel)} bytes / {sectors} sectors")
    Path(a.output).write_bytes(image)
    print(f"wrote {a.output}: {len(image)} bytes, stage2={len(stage2)} bytes")

if __name__=="__main__":main()
