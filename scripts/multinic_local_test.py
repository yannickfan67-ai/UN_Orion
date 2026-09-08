#!/usr/bin/env python3
import http.server, pathlib, shutil, socket, subprocess, tempfile, threading, time, os, sys
ROOT=pathlib.Path('/mnt/data/orion-netmulti')
IMG=ROOT/'build/orion-multinet.img'
Q='/mnt/data/qemu10root/usr/bin/qemu-system-x86_64'
CODE=pathlib.Path('/mnt/data/orion-ci/OVMF_CODE.localtest.fd')
VARS=pathlib.Path('/mnt/data/orion-ci/OVMF_VARS.localtest.fd')
MODELS=sys.argv[1:] or ['rtl8139','pcnet','e1000-82540em','e1000-82544gc','e1000-82545em','e1000e','virtio-net-pci']
class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self,*a): pass
site=pathlib.Path('/mnt/data/orion-multinet-site');site.mkdir(exist_ok=True);(site/'index.html').write_text('<html><body><h1>UN Orion multi NIC</h1><p>HTTP driver test passed</p></body></html>')
handler=lambda *a,**kw:Quiet(*a,directory=str(site),**kw)
srv=http.server.ThreadingHTTPServer(('0.0.0.0',18080),handler);threading.Thread(target=srv.serve_forever,daemon=True).start()
def hmp(path,cmd):
    s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);s.connect(str(path));s.settimeout(.2)
    try:s.recv(4096)
    except:pass
    s.sendall((cmd+'\n').encode());time.sleep(.03)
    try:s.recv(4096)
    except:pass
    s.close()
def modelarg(m):
    if m=='virtio-net-pci': return 'virtio-net-pci,disable-modern=on,disable-legacy=off,netdev=n0,romfile='
    return f'{m},netdev=n0,romfile='
res=[]
for m in MODELS:
  with tempfile.TemporaryDirectory(prefix='orion-nic-') as td:
    td=pathlib.Path(td); vf=td/'vars.fd'; shutil.copyfile(VARS,vf); ser=td/'serial.log'; mon=td/'mon.sock'
    env=os.environ.copy();env['LD_LIBRARY_PATH']='/mnt/data/qemu10root/usr/lib/x86_64-linux-gnu:'+env.get('LD_LIBRARY_PATH','')
    cmd=[Q,'-machine','q35','-m','256M','-drive',f'if=pflash,format=raw,readonly=on,file={CODE}','-drive',f'if=pflash,format=raw,file={vf}','-drive',f'format=raw,file={IMG}','-netdev','user,id=n0','-device',modelarg(m),'-display','none','-serial',f'file:{ser}','-monitor',f'unix:{mon},server,nowait','-no-reboot']
    p=subprocess.Popen(cmd,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,env=env,text=True)
    ok=False; reason=''
    try:
      dl=time.time()+10
      while time.time()<dl:
        text=ser.read_text(errors='ignore') if ser.exists() else ''
        if 'Orion desktop ready' in text:break
        if p.poll() is not None: reason='qemu exit '+(p.stderr.read()[-500:] if p.stderr else '');break
        time.sleep(.08)
      else: reason='boot timeout'
      if not reason:
        for k in ['n','e','t','t','e','s','t','ret']:
          hmp(mon,'sendkey '+k);time.sleep(.04)
        dl=time.time()+10
        while time.time()<dl:
          text=ser.read_text(errors='ignore') if ser.exists() else ''
          if 'HTTP test success' in text:ok=True;break
          if 'HTTP test failed' in text: reason='HTTP test failed';break
          if p.poll() is not None: reason='qemu exited';break
          time.sleep(.08)
        if not ok and not reason:reason='HTTP timeout'
      text=ser.read_text(errors='ignore') if ser.exists() else ''
      lines=[x for x in text.splitlines() if 'NET' in x or 'HTTP' in x or 'Orion desktop ready' in x]
      print(f'[{"PASS" if ok else "FAIL"}] {m}: {reason or "end-to-end HTTP"}')
      for x in lines[-12:]:print('   ',x)
      res.append((m,ok,reason,text))
    finally:
      p.terminate()
      try:p.wait(1)
      except: p.kill()
srv.shutdown()
print('\nSUMMARY')
for m,ok,r,_ in res: print(('PASS' if ok else 'FAIL'),m,r)
sys.exit(0 if all(x[1] for x in res) else 1)
