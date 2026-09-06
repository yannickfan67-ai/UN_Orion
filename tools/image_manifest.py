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
    p.add_argument('--version',default='0.0.5')
    args=p.parse_args()
    src=Path(args.input)
    if not src.is_file(): raise SystemExit(f'missing media: {src}')
    a=ARCHES[args.arch]
    manifest={
        'schema':'org.un.orion.image-manifest',
        'schema_version':1,
        'product':'UN_Orion',
        'version':args.version,
        'media_type':args.media,
        'file':{'name':src.name,'size':src.stat().st_size,'sha256':sha256(src)},
        'architecture':{'name':args.arch,'id':a['id'],'bits':a['bits'],'endianness':a['endian']},
        'boot':{
            'firmware':'uefi',
            'firmware_id':2,
            'uefi_machine':'x64',
            'secure_boot_required':False,
            'kernel_format':'elf64',
            'bootinfo_abi':{'major':1,'minor':0},
        },
        'minimum_contract':{
            'ram_mib':256,
            'framebuffer':['uefi-gop-rgb','uefi-gop-bgr'],
            'input':['ps2-keyboard','ps2-mouse-optional'],
            'network':['rtl8139-optional'],
        },
        'capabilities':[
            'desktop','installer' if args.media=='installer-iso' else 'bootable-disk',
            'orx-reserved','odi-reserved','network-ipv4','http-client','un-vela','aster-engine'
        ],
        'compatibility':{
            'policy':'Consumers must match schema major, architecture, firmware and required device contract before boot.',
            'future_architectures':['i686','aarch64','riscv64'],
        },
    }
    Path(args.output).write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n',encoding='utf-8')
    print(f'wrote {args.output}: {manifest["file"]["sha256"]}')

if __name__=='__main__': main()
