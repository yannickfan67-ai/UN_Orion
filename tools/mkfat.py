#!/usr/bin/env python3
"""Build a small FAT16 UEFI boot image without external mtools."""
import math, struct, sys
from pathlib import Path
if len(sys.argv)!=4:
    raise SystemExit(f"usage: {sys.argv[0]} OUTPUT BOOTX64.EFI KERNEL.ELF")
out, boot_path, kernel_path = map(Path, sys.argv[1:])
boot=boot_path.read_bytes(); kernel=kernel_path.read_bytes()
BPS=512; SPC=4; RESERVED=4; NFATS=2; ROOT_ENTRIES=512; FATSZ=128; TOTSEC=131072
rootsecs=(ROOT_ENTRIES*32+BPS-1)//BPS; data_start=RESERVED+NFATS*FATSZ+rootsecs; cb=BPS*SPC
image=bytearray(TOTSEC*BPS); bs=bytearray(512)
bs[0:3]=b'\xeb<\x90'; bs[3:11]=b'ORIONFS '; struct.pack_into('<H',bs,11,BPS); bs[13]=SPC
struct.pack_into('<H',bs,14,RESERVED); bs[16]=NFATS; struct.pack_into('<H',bs,17,ROOT_ENTRIES); bs[21]=0xF8
struct.pack_into('<H',bs,22,FATSZ); struct.pack_into('<H',bs,24,32); struct.pack_into('<H',bs,26,64); struct.pack_into('<I',bs,32,TOTSEC)
bs[36]=0x80; bs[38]=0x29; struct.pack_into('<I',bs,39,0x4F52494F); bs[43:54]=b'UN_ORION   '; bs[54:62]=b'FAT16   '; bs[510:512]=b'\x55\xaa'; image[:512]=bs
fat=[0]*((FATSZ*BPS)//2); fat[0]=0xFFF8; fat[1]=0xFFFF; nextcl=2
def alloc_dir():
    global nextcl
    c=nextcl; fat[c]=0xFFFF; nextcl+=1; return c
def alloc(blob):
    global nextcl
    n=max(1,math.ceil(len(blob)/cb)); start=nextcl
    if start+n>=len(fat)-2: raise SystemExit('image full')
    for i in range(n): fat[start+i]=0xFFFF if i==n-1 else start+i+1
    nextcl+=n; return start,n
cefi=alloc_dir(); cbootdir=alloc_dir(); cboot,nboot=alloc(boot); ckernel,nkernel=alloc(kernel)
fatbytes=bytearray(FATSZ*BPS)
for i,v in enumerate(fat):
    if i*2+2>len(fatbytes): break
    struct.pack_into('<H',fatbytes,i*2,v)
for fi in range(NFATS): image[(RESERVED+fi*FATSZ)*BPS:(RESERVED+(fi+1)*FATSZ)*BPS]=fatbytes
root_off=(RESERVED+NFATS*FATSZ)*BPS
def ent(name,ext,attr,cluster,size):
    e=bytearray(32); e[:8]=name.ljust(8).encode('ascii'); e[8:11]=ext.ljust(3).encode('ascii'); e[11]=attr
    struct.pack_into('<H',e,26,cluster); struct.pack_into('<I',e,28,size); return e
image[root_off:root_off+32]=ent('EFI','',0x10,cefi,0); image[root_off+32:root_off+64]=ent('KERNEL','ELF',0x20,ckernel,len(kernel))
data_off=data_start*BPS
def write_cluster(c,blob): image[data_off+(c-2)*cb:data_off+(c-2)*cb+len(blob)]=blob
buf=bytearray(cb); buf[:32]=ent('.','',0x10,cefi,0); buf[32:64]=ent('..','',0x10,0,0); buf[64:96]=ent('BOOT','',0x10,cbootdir,0); write_cluster(cefi,buf)
buf=bytearray(cb); buf[:32]=ent('.','',0x10,cbootdir,0); buf[32:64]=ent('..','',0x10,cefi,0); buf[64:96]=ent('BOOTX64','EFI',0x20,cboot,len(boot)); write_cluster(cbootdir,buf)
for i in range(nboot): write_cluster(cboot+i,boot[i*cb:(i+1)*cb])
for i in range(nkernel): write_cluster(ckernel+i,kernel[i*cb:(i+1)*cb])
out.parent.mkdir(parents=True,exist_ok=True); out.write_bytes(image)
print(f"Built {out} ({len(image)//(1024*1024)} MiB, kernel {len(kernel)} bytes)")
