#!/usr/bin/env python3
import http.server, os, pathlib, shutil, socket, subprocess, tempfile, threading, time, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
IMG=BUILD/'orion.img'
if not IMG.exists():
    raise SystemExit('build/orion.img missing; run make image first')

def find_ovmf(env_name,patterns):
    ev=os.environ.get(env_name)
    if ev and pathlib.Path(ev).exists(): return pathlib.Path(ev)
    roots=[pathlib.Path('/usr/share/OVMF'),pathlib.Path('/usr/share/edk2/ovmf')]
    for root in roots:
        for name in patterns:
            q=root/name
            if q.exists(): return q
    raise SystemExit(f'{env_name} firmware not found')
code=find_ovmf('OVMF_CODE',['OVMF_CODE_4M.fd','OVMF_CODE.fd'])
vars_template=find_ovmf('OVMF_VARS',['OVMF_VARS_4M.fd','OVMF_VARS.fd'])

class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args): pass

def serve(directory):
    handler=lambda *a,**kw: Quiet(*a,directory=str(directory),**kw)
    srv=http.server.ThreadingHTTPServer(('0.0.0.0',18080),handler)
    threading.Thread(target=srv.serve_forever,daemon=True).start()
    return srv

def hmp(sock,cmd):
    s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM); s.connect(str(sock)); s.recv(4096); s.sendall((cmd+'\n').encode()); time.sleep(.05)
    try:s.recv(4096)
    except OSError:pass
    s.close()

site=ROOT/'testsite'; site.mkdir(exist_ok=True)
(site/'index.html').write_text(
    '<!doctype html><html><head><title>CI Orion HTTP</title><style>.hidden{display:none}</style></head>'
    '<body><header><h1>CI Orion HTTP</h1></header><main><section><h2>Transport</h2>'
    '<p>network browser smoke &amp; markup preservation passed</p><h3>Semantic HTML</h3>'
    '<blockquote>Aster receives markup, not flattened text.</blockquote>'
    '<script>var should_not_render = "<fake>";</script></section></main><footer>UN_Vela</footer></body></html>',
    encoding='utf-8')
srv=serve(site)
with tempfile.TemporaryDirectory(prefix='orion-net-') as td:
    td=pathlib.Path(td); vars_fd=td/'vars.fd'; shutil.copyfile(vars_template,vars_fd); serial=td/'serial.log'; mon=td/'mon.sock'
    qemu=subprocess.Popen(['qemu-system-x86_64','-machine','q35','-m','256M','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={vars_fd}','-drive',f'format=raw,file={IMG}','-netdev','user,id=n0','-device','rtl8139,netdev=n0,romfile=','-display','none','-serial',f'file:{serial}','-monitor',f'unix:{mon},server,nowait'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        deadline=time.time()+12
        while time.time()<deadline:
            text=serial.read_text(errors='ignore') if serial.exists() else ''
            if 'Orion desktop ready' in text: break
            if qemu.poll() is not None: raise RuntimeError('QEMU exited during boot')
            time.sleep(.1)
        else: raise RuntimeError('boot timeout')
        for k in ['n','e','t','t','e','s','t','ret']:
            hmp(mon,'sendkey '+k); time.sleep(.07)
        deadline=time.time()+12
        while time.time()<deadline:
            text=serial.read_text(errors='ignore') if serial.exists() else ''
            if 'HTTP test failed' in text: raise RuntimeError(text[-1600:])
            if 'HTTP test success' in text and 'VELA: title CI Orion HTTP' in text and 'HTTP: markup response received' in text:
                print('UN_Orion network/browser smoke passed: RTL8139 -> TCP -> HTTP markup -> UN_Vela -> Aster title parse')
                sys.exit(0)
            time.sleep(.1)
        raise RuntimeError('HTTP/browser smoke timeout: '+(serial.read_text(errors='ignore')[-1600:] if serial.exists() else ''))
    finally:
        qemu.terminate()
        try:qemu.wait(timeout=2)
        except subprocess.TimeoutExpired:qemu.kill()
        srv.shutdown()
