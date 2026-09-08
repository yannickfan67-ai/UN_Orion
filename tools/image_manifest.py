#!/usr/bin/env python3
import argparse, hashlib, json
from pathlib import Path

ARCHES={
    'i686': {'id':1,'bits':32,'endian':'little'},
    'x86_64': {'id':2,'bits':64,'endian':'little'},
    'aarch64': {'id':3,'bits':64,'endian':'little'},
    'riscv64': {'id':4,'bits':64,'endian':'little'},
}

def sha256(path: Path) -> str:
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):
            h.update(chunk)
    return h.hexdigest()

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--input',required=True)
    p.add_argument('--output',required=True)
    p.add_argument('--media',choices=['disk-image','installer-iso'],required=True)
    p.add_argument('--arch',choices=ARCHES,default='x86_64')
    p.add_argument('--version',default='0.0.7')
    args=p.parse_args()
    src=Path(args.input)
    if not src.is_file(): raise SystemExit(f'missing media: {src}')
    a=ARCHES[args.arch]

    if args.arch == 'i686':
        boot={
            'firmware':'bios',
            'firmware_id':1,
            'secure_boot_required':False,
            'kernel_format':'flat32',
            'kernel_load_address':'0x00100000',
            'bootinfo_abi':{'major':1,'minor':0},
        }
        minimum={
            'ram_mib':64,
            'framebuffer':['vbe-lfb-32bpp'],
            'input':['ps2-keyboard','ps2-mouse-optional'],
            'network':['rtl8139-optional','pcnet-optional','e1000-optional','virtio-net-optional'],
        }
        caps=[
            'legacy-bios','protected-mode','cpuid','vbe-lfb','desktop','pmm','ps2-input',
            'netdev-abi','network-ipv4','http-client','un-vela','aster-engine','bootable-disk'
        ]
        future=['aarch64','riscv64']
    else:
        boot={
            'firmware':'uefi',
            'firmware_id':2,
            'uefi_machine':'x64',
            'secure_boot_required':False,
            'kernel_format':'elf64',
            'bootinfo_abi':{'major':1,'minor':0},
        }
        minimum={
            'ram_mib':256,
            'framebuffer':['uefi-gop-rgb','uefi-gop-bgr'],
            'input':['ps2-keyboard','ps2-mouse-optional'],
            'network':['rtl8139-optional','pcnet-optional','e1000-optional','virtio-net-optional'],
        }
        caps=[
            'desktop','installer' if args.media=='installer-iso' else 'bootable-disk',
            'orx-reserved','odi-reserved','netdev-abi','network-ipv4','http-client','un-vela','aster-engine'
        ]
        future=['aarch64','riscv64']

    manifest={
        'schema':'org.un.orion.image-manifest',
        'schema_version':1,
        'product':'UN_Orion',
        'version':args.version,
        'media_type':args.media,
        'file':{'name':src.name,'size':src.stat().st_size,'sha256':sha256(src)},
        'architecture':{'name':args.arch,'id':a['id'],'bits':a['bits'],'endianness':a['endian']},
        'boot':boot,
        'minimum_contract':minimum,
        'capabilities':caps,
        'compatibility':{
            'policy':'Consumers must match schema major, architecture, firmware and required device contract before boot.',
            'future_architectures':future,
        },
    }
    Path(args.output).write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n',encoding='utf-8')
    print(f'wrote {args.output}: {manifest["file"]["sha256"]}')

if __name__=='__main__': main()
