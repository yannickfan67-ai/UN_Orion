from pathlib import Path


def must_replace(path, old, new):
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"missing patch anchor in {path}: {old[:80]!r}")
    text = text.replace(old, new)
    p.write_text(text)

# Wire browser resource/image units into both x86_64 and derived i686 builds.
must_replace(
    "Makefile",
    "kernel/netdev_virtio.c kernel/net.c kernel/aster.c kernel/vela.c",
    "kernel/netdev_virtio.c kernel/net.c kernel/net_resource.c kernel/aster.c kernel/vela.c kernel/vela_image.c kernel/browser_image.c",
)

# Keep the wire User-Agent aligned with the embedded runtime.
must_replace(
    "kernel/net.c",
    "User-Agent: UN_Vela/0.1 Aster/0.1.1",
    "User-Agent: UN_Orion/0.0.7 UN_Vela/0.3 Aster/0.3",
)

# Desktop: 4-byte IntelliMouse packet support.
must_replace(
    "kernel/desktop.c",
    "static uint8_t mpkt[3];static int mpkt_i;",
    "static uint8_t mpkt[4];static int mpkt_i;",
)

# Browser chrome: Back / Forward / Reload + address + Go.
old_draw = '''static void draw_browser(Window*w){\n    int bx=w->x+1,by=w->y+TITLE_H+1,bw=w->w-2,bh=w->h-TITLE_H-2;gfx_rect(bx,by,bw,bh,0xF5F7FA);gfx_rect(bx,by,bw,58,0xDCE6EF);\n    gfx_rect(bx+14,by+10,bw-108,34,0xFFFFFF);const char*u=vela_url();gfx_text(bx+24,by+11,*u?u:"Enter HTTP address...",*u?0x26394B:0x8C9AAA,1);\n    gfx_rect(bx+bw-84,by+10,70,34,0x3578B7);gfx_text(bx+bw-63,by+11,"Go",0xFFFFFF,1);\n    char engine[128]="UN_Vela ";append(engine,VELA_VERSION,sizeof(engine));append(engine,"  /  Aster ",sizeof(engine));append(engine,aster_version(),sizeof(engine));gfx_text(bx+16,by+64,engine,0x4F6C86,1);\n    gfx_text_right(bx+bw-16,by+64,vela_status(),0x70869A);\n    gfx_line_h(bx+14,by+92,bw-28,0xD3DDE6);vela_paint(bx,by+98,bw,bh-104);\n}'''
new_draw = '''static void draw_browser(Window*w){\n    int bx=w->x+1,by=w->y+TITLE_H+1,bw=w->w-2,bh=w->h-TITLE_H-2;gfx_rect(bx,by,bw,bh,0xF5F7FA);gfx_rect(bx,by,bw,58,0xDCE6EF);\n    gfx_rect(bx+12,by+10,34,34,vela_can_back()?0x315F88:0xAAB8C4);gfx_text(bx+23,by+11,"<",0xFFFFFF,1);\n    gfx_rect(bx+52,by+10,34,34,vela_can_forward()?0x315F88:0xAAB8C4);gfx_text(bx+63,by+11,">",0xFFFFFF,1);\n    gfx_rect(bx+92,by+10,42,34,0x6B8195);gfx_text(bx+107,by+11,"R",0xFFFFFF,1);\n    int aw=bw-238;if(aw<120)aw=120;gfx_rect(bx+144,by+10,aw,34,0xFFFFFF);const char*u=vela_url();gfx_text(bx+154,by+11,*u?u:"Enter HTTP/HTTPS address...",*u?0x26394B:0x8C9AAA,1);\n    gfx_rect(bx+bw-84,by+10,70,34,0x3578B7);gfx_text(bx+bw-63,by+11,"Go",0xFFFFFF,1);\n    char engine[128]="UN_Vela ";append(engine,VELA_VERSION,sizeof(engine));append(engine,"  /  Aster ",sizeof(engine));append(engine,aster_version(),sizeof(engine));gfx_text(bx+16,by+64,engine,0x4F6C86,1);\n    gfx_text_right(bx+bw-16,by+64,vela_status(),0x70869A);\n    gfx_line_h(bx+14,by+92,bw-28,0xD3DDE6);vela_paint(bx,by+98,bw,bh-104);\n}'''
must_replace("kernel/desktop.c", old_draw, new_draw)

# Browser clicks: toolbar plus document link activation.
old_click = '''if(app==APP_BROWSER){int bx=w->x+1,by=w->y+TITLE_H+1,bw=w->w-2;if(inside(mouse_x,mouse_y,bx+bw-84,by+10,70,34)){vela_go();redraw();return;}}'''
new_click = '''if(app==APP_BROWSER){int bx=w->x+1,by=w->y+TITLE_H+1,bw=w->w-2,bh=w->h-TITLE_H-2;if(inside(mouse_x,mouse_y,bx+12,by+10,34,34)){if(vela_can_back())vela_back();redraw();return;}if(inside(mouse_x,mouse_y,bx+52,by+10,34,34)){if(vela_can_forward())vela_forward();redraw();return;}if(inside(mouse_x,mouse_y,bx+92,by+10,42,34)){vela_reload();redraw();return;}if(inside(mouse_x,mouse_y,bx+bw-84,by+10,70,34)){vela_go();redraw();return;}if(inside(mouse_x,mouse_y,bx+18,by+108,bw-36,bh-124)){vela_activate_link(mouse_x-(bx+18),mouse_y-(by+108));redraw();return;}}'''
must_replace("kernel/desktop.c", old_click, new_click)

# Wheel handling: signed low nibble in IntelliMouse packet, only when pointer is over browser document.
old_packet = '''static void handle_mouse_packet(void){\n    uint8_t b0=mpkt[0];int dx=(int8_t)mpkt[1],dy=(int8_t)mpkt[2];int oldx=mouse_x,oldy=mouse_y;mouse_prev_left=mouse_left;mouse_left=b0&1;cursor_restore();mouse_x+=dx;mouse_y-=dy;int maxx=(int)gfx_width()-16,maxy=(int)gfx_height()-24;if(mouse_x<0)mouse_x=0;if(mouse_y<0)mouse_y=0;if(mouse_x>maxx)mouse_x=maxx;if(mouse_y>maxy)mouse_y=maxy;\n    int needs_redraw=0;if(dragging>=0&&mouse_left){Window*w=&wins[dragging];w->x=mouse_x-drag_dx;w->y=mouse_y-drag_dy;if(w->x<0)w->x=0;if(w->y<0)w->y=0;if(w->x+w->w>(int)gfx_width())w->x=(int)gfx_width()-w->w;if(w->y+w->h>(int)gfx_height()-TASKBAR_H)w->y=(int)gfx_height()-TASKBAR_H-w->h;needs_redraw=1;}\n    if(mouse_prev_left&&!mouse_left)dragging=-1;if(!mouse_prev_left&&mouse_left){cursor_refresh();handle_press();return;}\n    if(mouse_left&&mouse_prev_left&&active_app==APP_PAINT&&dragging<0)paint_line(oldx,oldy,mouse_x,mouse_y);if(needs_redraw){draw_wallpaper();draw_desktop_icons();draw_windows();draw_start_menu();draw_taskbar();}cursor_refresh();\n}'''
new_packet = '''static void handle_mouse_packet(void){\n    uint8_t b0=mpkt[0];int dx=(int8_t)mpkt[1],dy=(int8_t)mpkt[2],wheel=0;if(mouse_packet_size()==4){wheel=mpkt[3]&15;if(wheel&8)wheel-=16;}int oldx=mouse_x,oldy=mouse_y;mouse_prev_left=mouse_left;mouse_left=b0&1;cursor_restore();mouse_x+=dx;mouse_y-=dy;int maxx=(int)gfx_width()-16,maxy=(int)gfx_height()-24;if(mouse_x<0)mouse_x=0;if(mouse_y<0)mouse_y=0;if(mouse_x>maxx)mouse_x=maxx;if(mouse_y>maxy)mouse_y=maxy;\n    if(wheel&&wins[APP_BROWSER].open&&!wins[APP_BROWSER].minimized){Window*b=&wins[APP_BROWSER];int bx=b->x+1,by=b->y+TITLE_H+1,bw=b->w-2,bh=b->h-TITLE_H-2;if(inside(mouse_x,mouse_y,bx+18,by+108,bw-36,bh-124)){vela_scroll_by(-wheel*48);redraw();return;}}\n    int needs_redraw=0;if(dragging>=0&&mouse_left){Window*w=&wins[dragging];w->x=mouse_x-drag_dx;w->y=mouse_y-drag_dy;if(w->x<0)w->x=0;if(w->y<0)w->y=0;if(w->x+w->w>(int)gfx_width())w->x=(int)gfx_width()-w->w;if(w->y+w->h>(int)gfx_height()-TASKBAR_H)w->y=(int)gfx_height()-TASKBAR_H-w->h;needs_redraw=1;}\n    if(mouse_prev_left&&!mouse_left)dragging=-1;if(!mouse_prev_left&&mouse_left){cursor_refresh();handle_press();return;}\n    if(mouse_left&&mouse_prev_left&&active_app==APP_PAINT&&dragging<0)paint_line(oldx,oldy,mouse_x,mouse_y);if(needs_redraw){draw_wallpaper();draw_desktop_icons();draw_windows();draw_start_menu();draw_taskbar();}cursor_refresh();\n}'''
must_replace("kernel/desktop.c", old_packet, new_packet)

# Packet assembler switches dynamically between standard and IntelliMouse mode.
must_replace(
    "kernel/desktop.c",
    "void desktop_mouse_byte(uint8_t b){if(mpkt_i==0&&!(b&0x08))return;mpkt[mpkt_i++]=b;if(mpkt_i==3){mpkt_i=0;handle_mouse_packet();}}",
    "void desktop_mouse_byte(uint8_t b){if(mpkt_i==0&&!(b&0x08))return;int need=mouse_packet_size();if(need!=4)need=3;mpkt[mpkt_i++]=b;if(mpkt_i>=need){mpkt_i=0;handle_mouse_packet();}}",
)

# Remove stale version labels.
must_replace("kernel/desktop.c", '"Browser: UN_Vela 0.1.0"', '"Browser: UN_Vela " VELA_VERSION')
must_replace("kernel/desktop.c", '"Engine: Aster 0.1.0"', '"Engine: Aster " ASTER_VERSION')
must_replace("kernel/desktop.c", '"UN_Orion " VERSION " / " ORION_ARCH_NAME " / UN_Vela 0.1 / Aster Engine 0.1"', '"UN_Orion " VERSION " / " ORION_ARCH_NAME " / UN_Vela " VELA_VERSION " / Aster " ASTER_VERSION')
must_replace("kernel/desktop.c", '"UN_Vela 0.1 now uses Aster Engine 0.1."', '"UN_Vela " VELA_VERSION " uses Aster Engine " ASTER_VERSION "."')

print("browser runtime integration patch applied")
