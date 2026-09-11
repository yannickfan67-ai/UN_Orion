#!/usr/bin/env python3
import hashlib, pathlib, shutil, sys, tarfile, urllib.request
URL='https://bearssl.org/bearssl-0.6.tar.gz'
SHA256='6705bba1714961b41a728dfc5debbe348d2966c117649392f8c8139efc83ff14'
out=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'build/vendor')
src=out/'bearssl-0.6'
stamp=src/'.orion-ready'
if stamp.exists(): raise SystemExit(0)
out.mkdir(parents=True,exist_ok=True)
archive=out/'bearssl-0.6.tar.gz'
if not archive.exists():
    with urllib.request.urlopen(URL,timeout=45) as r, archive.open('wb') as f: shutil.copyfileobj(r,f)
h=hashlib.sha256(archive.read_bytes()).hexdigest()
if h!=SHA256: raise SystemExit(f'BearSSL SHA256 mismatch: {h}')
with tarfile.open(archive,'r:gz') as tf:
    root=out.resolve()
    for m in tf.getmembers():
        dest=(out/m.name).resolve()
        if root not in dest.parents and dest!=root: raise SystemExit('unsafe tar path')
    tf.extractall(out,filter='data')
stamp.touch()
