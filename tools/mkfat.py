#!/usr/bin/env python3
"""Build a compact FAT16 UEFI boot image without external mtools.

The image used to be hard-coded to 64 MiB even when the bootloader and kernel
needed well under 1 MiB.  Keep a standards-compliant FAT16 filesystem, but pick
the smallest power-of-two image that leaves useful growth room for the payload.
"""
import math
import struct
import sys
from pathlib import Path

if len(sys.argv) != 4:
    raise SystemExit(f"usage: {sys.argv[0]} OUTPUT BOOTX64.EFI KERNEL.ELF")

out, boot_path, kernel_path = map(Path, sys.argv[1:])
boot = boot_path.read_bytes()
kernel = kernel_path.read_bytes()

BPS = 512
RESERVED = 4
NFATS = 2
ROOT_ENTRIES = 512
ROOT_SECS = (ROOT_ENTRIES * 32 + BPS - 1) // BPS
FAT16_MIN_CLUSTERS = 4085
FAT16_MAX_CLUSTERS = 65524
MIN_IMAGE_MIB = 4
MAX_IMAGE_MIB = 512


def geometry(total_bytes):
    """Return a valid FAT16 geometry for total_bytes, or None."""
    total_sectors = total_bytes // BPS
    for spc in (1, 2, 4, 8, 16, 32, 64, 128):
        fat_sectors = 1
        for _ in range(16):
            data_sectors = total_sectors - RESERVED - NFATS * fat_sectors - ROOT_SECS
            if data_sectors <= 0:
                break
            clusters = data_sectors // spc
            needed_fat = math.ceil((clusters + 2) * 2 / BPS)
            if needed_fat == fat_sectors:
                break
            fat_sectors = needed_fat
        else:
            continue

        data_sectors = total_sectors - RESERVED - NFATS * fat_sectors - ROOT_SECS
        if data_sectors <= 0:
            continue
        clusters = data_sectors // spc
        if FAT16_MIN_CLUSTERS <= clusters <= FAT16_MAX_CLUSTERS:
            return total_sectors, spc, fat_sectors, clusters
    return None


def choose_geometry():
    size_mib = MIN_IMAGE_MIB
    while size_mib <= MAX_IMAGE_MIB:
        g = geometry(size_mib * 1024 * 1024)
        if g is not None:
            total_sectors, spc, fat_sectors, cluster_count = g
            cluster_bytes = BPS * spc
            required_clusters = (
                2  # EFI and EFI/BOOT directories
                + max(1, math.ceil(len(boot) / cluster_bytes))
                + max(1, math.ceil(len(kernel) / cluster_bytes))
            )
            # Keep at least 25% of data clusters free so normal kernel growth
            # does not immediately force a larger disk image.
            if required_clusters * 4 <= cluster_count * 3:
                return size_mib, total_sectors, spc, fat_sectors, cluster_count
        size_mib *= 2
    raise SystemExit('payload is too large for the supported compact FAT16 image sizes')


IMAGE_MIB, TOTSEC, SPC, FATSZ, DATA_CLUSTERS = choose_geometry()
CB = BPS * SPC
DATA_START = RESERVED + NFATS * FATSZ + ROOT_SECS

image = bytearray(TOTSEC * BPS)
bs = bytearray(BPS)
bs[0:3] = b'\xeb<\x90'
bs[3:11] = b'ORIONFS '
struct.pack_into('<H', bs, 11, BPS)
bs[13] = SPC
struct.pack_into('<H', bs, 14, RESERVED)
bs[16] = NFATS
struct.pack_into('<H', bs, 17, ROOT_ENTRIES)
if TOTSEC < 0x10000:
    struct.pack_into('<H', bs, 19, TOTSEC)
    struct.pack_into('<I', bs, 32, 0)
else:
    struct.pack_into('<H', bs, 19, 0)
    struct.pack_into('<I', bs, 32, TOTSEC)
bs[21] = 0xF8
struct.pack_into('<H', bs, 22, FATSZ)
struct.pack_into('<H', bs, 24, 32)
struct.pack_into('<H', bs, 26, 64)
bs[36] = 0x80
bs[38] = 0x29
struct.pack_into('<I', bs, 39, 0x4F52494F)
bs[43:54] = b'UN_ORION   '
bs[54:62] = b'FAT16   '
bs[510:512] = b'\x55\xaa'
image[:BPS] = bs

fat_entries = (FATSZ * BPS) // 2
fat = [0] * fat_entries
fat[0] = 0xFFF8
fat[1] = 0xFFFF
nextcl = 2


def ensure_clusters(count):
    if count < 1:
        raise SystemExit('invalid cluster allocation')
    end = nextcl + count
    if end > DATA_CLUSTERS + 2 or end > len(fat):
        raise SystemExit('image full')


def alloc_dir():
    global nextcl
    ensure_clusters(1)
    c = nextcl
    fat[c] = 0xFFFF
    nextcl += 1
    return c


def alloc(blob):
    global nextcl
    n = max(1, math.ceil(len(blob) / CB))
    ensure_clusters(n)
    start = nextcl
    for i in range(n):
        fat[start + i] = 0xFFFF if i == n - 1 else start + i + 1
    nextcl += n
    return start, n


cefi = alloc_dir()
cbootdir = alloc_dir()
cboot, nboot = alloc(boot)
ckernel, nkernel = alloc(kernel)

fatbytes = bytearray(FATSZ * BPS)
for i, value in enumerate(fat):
    struct.pack_into('<H', fatbytes, i * 2, value)
for fi in range(NFATS):
    start = (RESERVED + fi * FATSZ) * BPS
    image[start:start + len(fatbytes)] = fatbytes

root_off = (RESERVED + NFATS * FATSZ) * BPS


def ent(name, ext, attr, cluster, size):
    e = bytearray(32)
    e[:8] = name.ljust(8).encode('ascii')
    e[8:11] = ext.ljust(3).encode('ascii')
    e[11] = attr
    struct.pack_into('<H', e, 26, cluster)
    struct.pack_into('<I', e, 28, size)
    return e


image[root_off:root_off + 32] = ent('EFI', '', 0x10, cefi, 0)
image[root_off + 32:root_off + 64] = ent('KERNEL', 'ELF', 0x20, ckernel, len(kernel))

data_off = DATA_START * BPS


def write_cluster(cluster, blob):
    off = data_off + (cluster - 2) * CB
    image[off:off + len(blob)] = blob


buf = bytearray(CB)
buf[:32] = ent('.', '', 0x10, cefi, 0)
buf[32:64] = ent('..', '', 0x10, 0, 0)
buf[64:96] = ent('BOOT', '', 0x10, cbootdir, 0)
write_cluster(cefi, buf)

buf = bytearray(CB)
buf[:32] = ent('.', '', 0x10, cbootdir, 0)
buf[32:64] = ent('..', '', 0x10, cefi, 0)
buf[64:96] = ent('BOOTX64', 'EFI', 0x20, cboot, len(boot))
write_cluster(cbootdir, buf)

for i in range(nboot):
    write_cluster(cboot + i, boot[i * CB:(i + 1) * CB])
for i in range(nkernel):
    write_cluster(ckernel + i, kernel[i * CB:(i + 1) * CB])

out.parent.mkdir(parents=True, exist_ok=True)
out.write_bytes(image)
used_clusters = nextcl - 2
print(
    f"Built {out} ({IMAGE_MIB} MiB FAT16, {used_clusters}/{DATA_CLUSTERS} clusters used, "
    f"kernel {len(kernel)} bytes)"
)
