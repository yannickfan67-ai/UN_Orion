from pathlib import Path

p = Path("kernel/vela.c")
text = p.read_text()
old = 'char body[4096]={0},w[2048]={0};scripts(r,rc,t,tc,body,sizeof(body),w,sizeof(w));'
new = 'char body[4096],w[2048];zero(body,sizeof(body));zero(w,sizeof(w));scripts(r,rc,t,tc,body,sizeof(body),w,sizeof(w));'
if old not in text:
    raise SystemExit("Vela freestanding zero-init patch anchor not found")
p.write_text(text.replace(old, new))
print("removed implicit libc memset dependency")
