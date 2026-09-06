#!/usr/bin/env python3
"""Build a minimal UEFI El Torito ISO around the existing UN_Orion FAT image.
Pure Python: no xorriso/genisoimage dependency.
"""
from pathlib import Path
import argparse, math, struct

BS=2048

def both16(v): return struct.pack('<H',v)+struct.pack('>H',v)
def both32(v): return struct.pack('<I',v)+struct.pack('>I',v)

def dirrec(lba,size,flags,file_id):
    fid=file_id if isinstance(file_id,bytes) else file_id.encode('ascii')
    n=33+len(fid)
    if n%2: n+=1
    r=bytearray(n); r[0]=n; r[1]=0
    r[2:10]=both32(lba); r[10:18]=both32(size)
    r[18:25]=bytes([126,9,6,16,0,0,0])
    r[25]=flags; r[28:32]=both16(1); r[32]=len(fid); r[33:33+len(fid)]=fid
    return bytes(r)

def pvd(total,root_lba,path_l,path_m):
    b=bytearray(BS); b[0]=1; b[1:6]=b'CD001'; b[6]=1
    b[8:40]=b'UN_ORION'.ljust(32,b' '); b[40:72]=b'UN_ORION_INSTALL'.ljust(32,b' ')
    b[80:88]=both32(total); b[120:124]=both16(1); b[124:128]=both16(1); b[128:132]=both16(BS)
    b[132:140]=both32(10); b[140:144]=struct.pack('<I',path_l); b[148:152]=struct.pack('>I',path_m)
    b[156:190]=dirrec(root_lba,BS,2,b'\x00')
    b[318:446]=b'UN Project'.ljust(128,b' '); b[446:574]=b'UN_Orion build tooling'.ljust(128,b' '); b[574:702]=b'UN_Orion UEFI Installer'.ljust(128,b' ')
    date=b'2026090616000000\x00'; b[813:830]=date; b[830:847]=date; b[847:864]=b'0'*16+b'\x00'; b[864:881]=b'0'*16+b'\x00'; b[881]=1
    return b

def boot_record(catalog_lba):
    b=bytearray(BS); b[0]=0; b[1:6]=b'CD001'; b[6]=1
    b[7:39]=b'EL TORITO SPECIFICATION'.ljust(32,b' '); b[71:75]=struct.pack('<I',catalog_lba)
    return b

def terminator():
    b=bytearray(BS); b[0]=255; b[1:6]=b'CD001'; b[6]=1; return b

def pathtable(root_lba,big=False):
    b=bytearray(BS); b[0]=1
    b[2:6]=struct.pack('>I' if big else '<I',root_lba); b[6:8]=struct.pack('>H' if big else '<H',1)
    return b

def catalog(boot_lba):
    b=bytearray(BS)
    v=bytearray(32); v[0]=1; v[1]=0xEF; v[4:28]=b'UN_Orion UEFI'.ljust(24,b' '); v[30]=0x55; v[31]=0xAA
    v[28:30]=struct.pack('<H',(-sum(struct.unpack('<16H',v)))&0xffff); b[:32]=v
    e=bytearray(32); e[0]=0x88; e[1]=0; e[6:8]=struct.pack('<H',1); e[8:12]=struct.pack('<I',boot_lba); b[32:64]=e
    return b

def build(src,dst):
    img=Path(src).read_bytes(); readme=b'UN_Orion v0.0.5 UEFI install/live media\r\nBoots the current system image through El Torito EFI.\r\n'
    path_l,path_m,root_lba,catalog_lba,readme_lba,boot_lba=19,20,21,22,23,24
    total=boot_lba+math.ceil(len(img)/BS); out=bytearray(total*BS)
    out[16*BS:17*BS]=pvd(total,root_lba,path_l,path_m); out[17*BS:18*BS]=boot_record(catalog_lba); out[18*BS:19*BS]=terminator()
    out[path_l*BS:(path_l+1)*BS]=pathtable(root_lba); out[path_m*BS:(path_m+1)*BS]=pathtable(root_lba,True)
    root=bytearray(BS); pos=0
    for rec in (dirrec(root_lba,BS,2,b'\x00'),dirrec(root_lba,BS,2,b'\x01'),dirrec(readme_lba,len(readme),0,b'README.TXT;1'),dirrec(boot_lba,len(img),0,b'ORION.IMG;1')):
        root[pos:pos+len(rec)]=rec; pos+=len(rec)
    out[root_lba*BS:(root_lba+1)*BS]=root; out[catalog_lba*BS:(catalog_lba+1)*BS]=catalog(boot_lba)
    out[readme_lba*BS:readme_lba*BS+len(readme)]=readme; out[boot_lba*BS:boot_lba*BS+len(img)]=img
    Path(dst).write_bytes(out); print(f'wrote {dst}: {len(out)} bytes')

if __name__=='__main__':
    ap=argparse.ArgumentParser(); ap.add_argument('image'); ap.add_argument('iso'); a=ap.parse_args(); build(a.image,a.iso)
