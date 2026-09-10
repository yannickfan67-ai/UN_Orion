#!/usr/bin/env python3
import http.server, os, pathlib, shutil, socket, ssl, subprocess, tempfile, threading, time, sys

ROOT=pathlib.Path(__file__).resolve().parents[1]

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

class ReuseHTTPServer(http.server.ThreadingHTTPServer):
    allow_reuse_address=True

class Redirect(http.server.BaseHTTPRequestHandler):
    def log_message(self, fmt, *args): pass
    def do_GET(self):
        self.send_response(302)
        self.send_header('Location','https://10.0.2.2:18443/final.html')
        self.send_header('Content-Length','0')
        self.end_headers()

class Site(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args): pass

def serve_http():
    srv=ReuseHTTPServer(('0.0.0.0',18080),Redirect)
    threading.Thread(target=srv.serve_forever,daemon=True).start()
    return srv

def serve_https(directory,cert,key):
    handler=lambda *a,**kw: Site(*a,directory=str(directory),**kw)
    srv=ReuseHTTPServer(('0.0.0.0',18443),handler)
    ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.minimum_version=ssl.TLSVersion.TLSv1_2
    ctx.maximum_version=ssl.TLSVersion.TLSv1_2
    ctx.load_cert_chain(certfile=str(cert),keyfile=str(key))
    srv.socket=ctx.wrap_socket(srv.socket,server_side=True)
    threading.Thread(target=srv.serve_forever,daemon=True).start()
    return srv

def run(*args,**kw):
    subprocess.run(args,check=True,**kw)

def make_test_ca(td):
    ca_key=td/'ca.key'; ca_crt=td/'ca.crt'; leaf_key=td/'server.key'; csr=td/'server.csr'; leaf_crt=td/'server.crt'; ext=td/'server.ext'
    run('openssl','req','-x509','-newkey','rsa:2048','-sha256','-nodes','-days','2',
        '-subj','/CN=UN Orion CI Root','-keyout',str(ca_key),'-out',str(ca_crt),
        stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    run('openssl','req','-new','-newkey','rsa:2048','-sha256','-nodes','-subj','/CN=10.0.2.2',
        '-keyout',str(leaf_key),'-out',str(csr),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    ext.write_text('basicConstraints=CA:FALSE\nkeyUsage=digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=IP:10.0.2.2\n',encoding='ascii')
    run('openssl','x509','-req','-in',str(csr),'-CA',str(ca_crt),'-CAkey',str(ca_key),'-CAcreateserial',
        '-days','2','-sha256','-extfile',str(ext),'-out',str(leaf_crt),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    return ca_crt,leaf_crt,leaf_key

def hmp(sock,cmd):
    s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM); s.connect(str(sock)); s.recv(4096); s.sendall((cmd+'\n').encode()); time.sleep(.05)
    try:s.recv(4096)
    except OSError:pass
    s.close()

with tempfile.TemporaryDirectory(prefix='orion-tls-net-') as tmp:
    td=pathlib.Path(tmp)
    ca_crt,leaf_crt,leaf_key=make_test_ca(td)
    test_build=td/'build'
    run('make',f'BUILD={test_build}',f'TLS_CA_BUNDLE={ca_crt}','image',cwd=ROOT)
    img=test_build/'orion.img'
    if not img.exists(): raise RuntimeError('TLS test image was not produced')

    site=td/'site'; site.mkdir()
    (site/'final.html').write_text(
        '<!doctype html><html><head><title>CI Orion HTTPS</title><style>.hidden{display:none}</style></head>'
        '<body><header><h1>CI Orion HTTPS</h1></header><main><section><h2>Transport</h2>'
        '<p>verified TLS 1.2 browser smoke &amp; markup preservation passed</p><h3>Semantic HTML</h3>'
        '<blockquote>Aster receives HTTPS markup, not flattened text.</blockquote>'
        '<script>var should_not_render = "<fake>";</script></section></main><footer>UN_Vela</footer></body></html>',
        encoding='utf-8')
    http_srv=serve_http(); https_srv=serve_https(site,leaf_crt,leaf_key)
    vars_fd=td/'vars.fd'; shutil.copyfile(vars_template,vars_fd); serial=td/'serial.log'; mon=td/'mon.sock'
    qemu=subprocess.Popen(['qemu-system-x86_64','-machine','q35','-m','256M',
        '-drive',f'if=pflash,format=raw,readonly=on,file={code}',
        '-drive',f'if=pflash,format=raw,file={vars_fd}',
        '-drive',f'format=raw,file={img}',
        '-netdev','user,id=n0','-device','rtl8139,netdev=n0,romfile=',
        '-display','none','-serial',f'file:{serial}','-monitor',f'unix:{mon},server,nowait'],
        stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
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
        deadline=time.time()+20
        while time.time()<deadline:
            text=serial.read_text(errors='ignore') if serial.exists() else ''
            if 'HTTP test failed' in text: raise RuntimeError(text[-2200:])
            if ('HTTP test success' in text and 'VELA: title CI Orion HTTPS' in text
                and 'HTTP: redirect https://10.0.2.2:18443/final.html' in text
                and 'TLS: verified HTTPS session' in text and 'HTTP: markup response received' in text):
                print('UN_Orion HTTPS smoke passed: RTL8139 -> HTTP 302 -> TLS 1.2 + CA/IP/time validation -> UN_Vela -> Aster')
                sys.exit(0)
            time.sleep(.1)
        raise RuntimeError('HTTPS/browser smoke timeout: '+(serial.read_text(errors='ignore')[-2200:] if serial.exists() else ''))
    finally:
        qemu.terminate()
        try:qemu.wait(timeout=2)
        except subprocess.TimeoutExpired:qemu.kill()
        http_srv.shutdown(); https_srv.shutdown()
