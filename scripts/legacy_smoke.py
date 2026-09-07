#!/usr/bin/env python3
"""Verify Legacy BIOS -> i686 protected mode -> ATA PIO using a disposable disk."""
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
FLOPPY = BUILD / "UN_Orion-i686-bios-smoke.img"
HDD = BUILD / "legacy-test-hdd.img"
SIGNATURE = b"ORIONI686-PMODE-BOOT-OK"

qemu = shutil.which("qemu-system-i386") or shutil.which("qemu-system-x86_64")
if not qemu:
    raise SystemExit("legacy smoke requires qemu-system-i386 or qemu-system-x86_64")

with HDD.open("wb") as f:
    f.truncate(32 * 1024 * 1024)

cmd = [
    qemu, "-machine", "pc", "-m", "64M",
    "-drive", f"if=floppy,format=raw,file={FLOPPY}",
    "-drive", f"if=ide,format=raw,file={HDD}",
    "-boot", "a", "-display", "none", "-serial", "none", "-monitor", "none",
    "-no-reboot",
]
proc = subprocess.Popen(cmd, cwd=ROOT)
deadline = time.time() + 10
ok = False
try:
    while time.time() < deadline:
        time.sleep(0.15)
        with HDD.open("rb") as f:
            f.seek(11 * 512)
            if f.read(len(SIGNATURE)) == SIGNATURE:
                ok = True
                break
finally:
    proc.terminate()
    try:
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()

if not ok:
    raise SystemExit("legacy i686 smoke failed: ATA LBA11 boot signature not observed")
print("UN_Orion legacy i686 smoke passed: BIOS -> protected mode -> CPUID -> ATA PIO")
