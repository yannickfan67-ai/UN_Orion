#include <stdint.h>
#include <stddef.h>
#include "bootinfo.h"
#include "graphics.h"
#include "interrupts.h"
#include "pmm.h"
#include "serial.h"
#include "io.h"
#include "desktop.h"
#include "net.h"
#include "vela.h"
#include "aster.h"
#include "version.h"

#define VERSION ORION_VERSION
#define TASKBAR_H 54
#define TITLE_H 42
#define APP_COUNT 7
#define APP_TERMINAL 0
#define APP_FILES 1
#define APP_NOTES 2
#define APP_PAINT 3
#define APP_BROWSER 4
#define APP_NETWORK 5
#define APP_ABOUT 6
#define TERM_LINES 13
#define TERM_LEN 76
#define NOTE_CAP 1536
#define PAINT_W 640
#define PAINT_H 360

typedef struct{int x,y,w,h;int open,minimized,z;const char *title;} Window;
static Window wins[APP_COUNT];
static int ztop=10;
static int active_app=-1;
static int start_open;
static int dragging=-1,drag_dx,drag_dy;
static int mouse_x=700,mouse_y=420;
static int mouse_left,mouse_prev_left;
static uint8_t mpkt[4];static int mpkt_i;
static int shift_down;
static uint64_t last_second=(uint64_t)-1;
static OrionBootInfo *g_bi;

static char term_lines[TERM_LINES][TERM_LEN];static int term_count;
static char term_cmd[64];static int term_len;
static char notes[NOTE_CAP]="Welcome to UN_Orion Notes.\n\nUN_Vela is now powered by the native Aster Engine.";static int note_len=78;
static uint8_t paint[PAINT_W*PAINT_H];
static int net_ping_result=-1;

static uint32_t cursor_under[16*24];static int cursor_saved,cursor_sx,cursor_sy;
static const uint16_t cursor_shape[18]={0x8000,0xC000,0xE000,0xF000,0xF800,0xFC00,0xFE00,0xFF00,0xFF80,0xF800,0xDC00,0x8E00,0x0600,0x0700,0x0300,0x0300,0x0000,0x0000};

static int inside(int x,int y,int rx,int ry,int rw,int rh){return x>=rx&&y>=ry&&x<rx+rw&&y<ry+rh;}
static int streq(const char*a,const char*b){while(*a&&*b&&*a==*b){a++;b++;}return *a==*b;}
static void append(char*a,const char*b,size_t cap){size_t i=0;while(i<cap&&a[i])i++;while(i+1<cap&&b&&*b)a[i++]=*b++;if(i<cap)a[i]=0;}
static void u64dec(uint64_t v,char*out){char t[24];int n=0;if(!v){out[0]='0';out[1]=0;return;}while(v){t[n++]=(char)('0'+v%10);v/=10;}for(int i=0;i<n;i++)out[i]=t[n-1-i];out[n]=0;}
static void hex64(uint64_t v,char out[19]){static const char h[]="0123456789ABCDEF";out[0]='0';out[1]='x';for(int i=0;i<16;i++)out[i+2]=h[(v>>(60-4*i))&15];out[18]=0;}

static void cursor_restore(void){if(!cursor_saved)return;for(int y=0;y<24;y++)for(int x=0;x<16;x++)gfx_put_raw((uint32_t)(cursor_sx+x),(uint32_t)(cursor_sy+y),cursor_under[y*16+x]);cursor_saved=0;}
static void cursor_capture(void){cursor_sx=mouse_x;cursor_sy=mouse_y;for(int y=0;y<24;y++)for(int x=0;x<16;x++)cursor_under[y*16+x]=gfx_get_raw((uint32_t)(mouse_x+x),(uint32_t)(mouse_y+y));cursor_saved=1;}
static void cursor_draw(void){for(int y=0;y<18;y++){uint16_t row=cursor_shape[y];for(int x=0;x<12;x++)if(row&(0x8000u>>x))gfx_pixel((uint32_t)(mouse_x+x),(uint32_t)(mouse_y+y),x<2||y<2?0xFFFFFF:0x111820);}}
static void cursor_refresh(void){cursor_capture();cursor_draw();}

static void draw_wallpaper(void){
    uint32_t W=gfx_width(),H=gfx_height();
    for(uint32_t y=0;y<H-TASKBAR_H;y+=8){uint32_t t=(y*100)/(H?H:1);uint32_t r=20-(t*7/100),g=35-(t*12/100),b=58-(t*17/100);gfx_rect(0,y,W,8,(r<<16)|(g<<8)|b);}
    gfx_rect(W>390?W-390:0,80,260,260,0x162B47);gfx_rect(W>355?W-355:0,115,190,190,0x183451);gfx_rect(W>320?W-320:0,150,120,120,0x1B3E5E);
    if(W>900){gfx_display_text(W-515,126,"ORION",0x274E73);gfx_text(W-510,196,"A DESKTOP BUILT DIRECTLY ON THE KERNEL",0x385F82,1);}
}
static void icon_box(int x,int y,const char*label,int kind){
    gfx_rect(x+10,y,48,48,0x1B2C42);
    if(kind==APP_TERMINAL){gfx_rect(x+19,y+13,30,22,0x0C1118);gfx_text(x+22,y+10,">_",0x78B8FF,1);}
    else if(kind==APP_FILES){gfx_rect(x+17,y+15,34,25,0xE5B95C);gfx_rect(x+20,y+10,16,8,0xF1CA74);}
    else if(kind==APP_NOTES){gfx_rect(x+18,y+9,32,34,0xF1F4F7);for(int i=0;i<4;i++)gfx_line_h(x+23,y+17+i*6,20,0x7990A7);}
    else if(kind==APP_PAINT){gfx_rect(x+18,y+10,32,32,0xF4F5F6);gfx_rect(x+23,y+15,8,8,0x62A8FF);gfx_rect(x+34,y+25,10,10,0xE07388);}
    else if(kind==APP_BROWSER){gfx_rect(x+17,y+10,34,34,0xF2F7FB);gfx_rect(x+21,y+14,26,26,0x4B9FEA);gfx_rect(x+30,y+14,8,26,0xEAF5FF);gfx_line_h(x+21,y+25,26,0xEAF5FF);}
    else if(kind==APP_NETWORK){gfx_rect(x+18,y+29,5,11,0x7FC5FF);gfx_rect(x+27,y+23,5,17,0x7FC5FF);gfx_rect(x+36,y+17,5,23,0x7FC5FF);gfx_rect(x+45,y+11,5,29,0x7FC5FF);}
    else {gfx_rect(x+18,y+10,32,32,0x223C5E);gfx_text(x+29,y+10,"i",0xD8E9FB,1);}
    gfx_text(x,y+53,label,0xE4ECF5,1);
}
static void draw_desktop_icons(void){icon_box(26,42,"Terminal",APP_TERMINAL);icon_box(26,133,"Files",APP_FILES);icon_box(26,224,"Notes",APP_NOTES);icon_box(26,315,"Paint",APP_PAINT);icon_box(26,406,"UN_Vela",APP_BROWSER);icon_box(26,497,"Network",APP_NETWORK);}

static void draw_taskbar(void){
    uint32_t W=gfx_width(),H=gfx_height(),y=H-TASKBAR_H;gfx_rect(0,y,W,TASKBAR_H,0x0C121B);gfx_line_h(0,y,W,0x33445A);
    gfx_rect(14,y+9,38,36,start_open?0x284B72:0x16263A);gfx_rect(23,y+17,20,20,0x5CA8FF);gfx_text(29,y+12,"O",0xF6FAFE,1);
    for(int i=0;i<APP_COUNT;i++){int x=70+i*48;if(wins[i].open&&!wins[i].minimized)gfx_rect(x,y+8,40,38,active_app==i?0x263D59:0x172638);else gfx_rect(x,y+8,40,38,0x111C2A);gfx_text(x+13,y+11,i==0?">":i==1?"F":i==2?"N":i==3?"P":i==4?"V":i==5?"W":"i",0xDDE8F3,1);}
    uint64_t sec=timer_frequency()?timer_ticks()/timer_frequency():0;char t[24]="UP ",n[20];u64dec(sec,n);append(t,n,sizeof(t));append(t,"s",sizeof(t));gfx_text_right(W-22,y+12,t,0xA4B5C8);
}
static void draw_start_menu(void){
    if(!start_open)return;uint32_t H=gfx_height();int x=14,y=(int)H-TASKBAR_H-438,w=280,h=428;gfx_rect(x+6,y+6,w,h,0x08101A);gfx_rect(x,y,w,h,0x142130);gfx_rect(x,y,w,58,0x19304A);gfx_text(x+20,y+14,"UN ORION",0xEDF5FD,1);gfx_text(x+20,y+38,"Desktop Preview " VERSION,0x7894B0,1);
    const char*names[APP_COUNT]={"Terminal","Files","Notes","Paint","UN_Vela","Network","About"};for(int i=0;i<APP_COUNT;i++){int iy=y+68+i*43;if(inside(mouse_x,mouse_y,x+10,iy,w-20,36))gfx_rect(x+10,iy,w-20,36,0x203A56);gfx_text(x+24,iy+4,names[i],0xE4EDF6,1);}
    gfx_line_h(x+14,y+382,w-28,0x314255);gfx_text(x+24,y+389,"Shut down",0xD5A0AA,1);
}
static void draw_window_frame(int app){
    Window *w=&wins[app];if(!w->open||w->minimized)return;
    gfx_rect(w->x+7,w->y+8,w->w,w->h,0x07101A);
    gfx_rect(w->x,w->y,w->w,TITLE_H,active_app==app?0x15283D:0x26313E);
    gfx_rect(w->x,w->y+TITLE_H,w->w,1,0xC5CED8);gfx_rect(w->x,w->y,1,w->h,0xC5CED8);gfx_rect(w->x+w->w-1,w->y,1,w->h,0xC5CED8);gfx_rect(w->x,w->y+w->h-1,w->w,1,0xC5CED8);
    gfx_text(w->x+16,w->y+7,w->title,0xF0F5FA,1);gfx_rect(w->x+w->w-40,w->y+5,34,31,inside(mouse_x,mouse_y,w->x+w->w-40,w->y+5,34,31)?0xA94052:0x263747);gfx_text(w->x+w->w-29,w->y+5,"x",0xF8EDF0,1);
}
static void term_push(const char*s){int row;if(term_count<TERM_LINES)row=term_count++;else{for(int i=0;i<TERM_LINES-1;i++)for(int j=0;j<TERM_LEN;j++)term_lines[i][j]=term_lines[i+1][j];row=TERM_LINES-1;}int j=0;while(s&&*s&&j<TERM_LEN-1)term_lines[row][j++]=*s++;term_lines[row][j]=0;}
static void draw_terminal(Window*w){int bx=w->x+1,by=w->y+TITLE_H+1,bw=w->w-2,bh=w->h-TITLE_H-2;gfx_rect(bx,by,bw,bh,0x0A1018);gfx_text(bx+18,by+14,"Orion Terminal",0x7FB9F4,1);gfx_text_right(bx+bw-18,by+14,"shell",0x5E748B);int y=by+48;for(int i=0;i<term_count;i++,y+=23)gfx_text(bx+18,y,term_lines[i],0xAABBCD,1);gfx_rect(bx+14,by+bh-42,bw-28,30,0x0E1722);gfx_text(bx+21,by+bh-39,"orion >",0x65AFFF,1);gfx_text(bx+116,by+bh-39,term_cmd,0xE6EEF6,1);}
static void draw_files(Window*w){int bx=w->x+1,by=w->y+TITLE_H+1,bw=w->w-2,bh=w->h-TITLE_H-2;gfx_rect(bx,by,bw,bh,0xF2F5F8);gfx_rect(bx,by,150,bh,0xE4EAF0);gfx_text(bx+18,by+16,"Home",0x253749,1);gfx_text(bx+18,by+54,"Desktop",0x5F7184,1);gfx_text(bx+18,by+87,"Documents",0x5F7184,1);gfx_text(bx+18,by+120,"System",0x5F7184,1);gfx_text(bx+178,by+18,"Home",0x213548,1);gfx_line_h(bx+175,by+50,bw-195,0xD0D9E2);icon_box(bx+185,by+70,"Notes.txt",APP_NOTES);icon_box(bx+290,by+70,"Canvas",APP_PAINT);icon_box(bx+395,by+70,"UN_Vela",APP_BROWSER);gfx_text(bx+180,by+175,"Desktop applications are native UN_Orion components.",0x66798D,1);}
static void draw_notes(Window*w){int bx=w->x+1,by=w->y+TITLE_H+1,bw=w->w-2,bh=w->h-TITLE_H-2;gfx_rect(bx,by,bw,bh,0xFAF9F2);gfx_rect(bx,by,bw,36,0xE7E4D8);gfx_text(bx+16,by+3,"Notes   editable RAM document",0x4A4B46,1);int x=bx+20,y=by+52,col=0;char one[2]={0,0};for(int i=0;i<note_len&&y<by+bh-28;i++){char c=notes[i];if(c=='\n'||col>58){y+=24;x=bx+20;col=0;if(c=='\n')continue;}one[0]=c;uint32_t adv=gfx_text((uint32_t)x,(uint32_t)y,one,0x303B43,1);x+=(int)adv;col++;}gfx_rect((uint32_t)x,(uint32_t)(y+25),10,2,0x406D9A);}
static void draw_paint(Window*w){int bx=w->x+1,by=w->y+TITLE_H+1,bw=w->w-2,bh=w->h-TITLE_H-2;gfx_rect(bx,by,bw,bh,0xE9EDF1);gfx_rect(bx,by,bw,46,0xD6DEE6);gfx_text(bx+16,by+7,"Brush",0x33485B,1);gfx_rect(bx+96,by+8,60,30,0xF4F7FA);gfx_text(bx+108,by+5,"Clear",0x33485B,1);int cx=bx+18,cy=by+60,cw=bw-36,ch=bh-78;if(cw>PAINT_W)cw=PAINT_W;if(ch>PAINT_H)ch=PAINT_H;gfx_rect(cx,cy,cw,ch,0xFFFFFF);for(int yy=0;yy<ch;yy++)for(int xx=0;xx<cw;xx++)if(paint[yy*PAINT_W+xx])gfx_pixel((uint32_t)(cx+xx),(uint32_t)(cy+yy),0x253A52);gfx_rect(cx,cy,cw,1,0xB9C3CC);gfx_rect(cx,cy,1,ch,0xB9C3CC);}
static void iptext(const uint8_t ip[4],char*out,size_t cap){char n[8];out[0]=0;for(int i=0;i<4;i++){u64dec(ip[i],n);append(out,n,cap);if(i<3)append(out,".",cap);}}
static void draw_browser(Window*w){
    int bx=w->x+1,by=w->y+TITLE_H+1,bw=w->w-2,bh=w->h-TITLE_H-2;gfx_rect(bx,by,bw,bh,0xF5F7FA);gfx_rect(bx,by,bw,58,0xDCE6EF);
    gfx_rect(bx+12,by+10,34,34,vela_can_back()?0x315F88:0xAAB8C4);gfx_text(bx+23,by+11,"<",0xFFFFFF,1);
    gfx_rect(bx+52,by+10,34,34,vela_can_forward()?0x315F88:0xAAB8C4);gfx_text(bx+63,by+11,">",0xFFFFFF,1);
    gfx_rect(bx+92,by+10,42,34,0x6B8195);gfx_text(bx+107,by+11,"R",0xFFFFFF,1);
    int aw=bw-238;if(aw<120)aw=120;gfx_rect(bx+144,by+10,aw,34,0xFFFFFF);const char*u=vela_url();gfx_text(bx+154,by+11,*u?u:"Enter HTTP/HTTPS address...",*u?0x26394B:0x8C9AAA,1);
    gfx_rect(bx+bw-84,by+10,70,34,0x3578B7);gfx_text(bx+bw-63,by+11,"Go",0xFFFFFF,1);
    char engine[128]="UN_Vela ";append(engine,VELA_VERSION,sizeof(engine));append(engine,"  /  Aster ",sizeof(engine));append(engine,aster_version(),sizeof(engine));gfx_text(bx+16,by+64,engine,0x4F6C86,1);
    gfx_text_right(bx+bw-16,by+64,vela_status(),0x70869A);
    gfx_line_h(bx+14,by+92,bw-28,0xD3DDE6);vela_paint(bx,by+98,bw,bh-104);
}
static void draw_network(Window*w){int bx=w->x+1,by=w->y+TITLE_H+1,bw=w->w-2,bh=w->h-TITLE_H-2;gfx_rect(bx,by,bw,bh,0xF0F4F8);gfx_text(bx+24,by+20,"Network",0x233A50,1);gfx_text(bx+24,by+58,net_ready()?"Connected":"No supported adapter",net_ready()?0x2A7B4F:0xA04455,1);char s[96]="Adapter: ";append(s,net_driver_name(),sizeof(s));gfx_text(bx+24,by+98,s,0x5C7184,1);uint8_t ip[4];char v[32];net_get_ipv4(ip);iptext(ip,v,sizeof(v));s[0]=0;append(s,"IPv4: ",sizeof(s));append(s,v,sizeof(s));gfx_text(bx+24,by+132,s,0x5C7184,1);net_get_gateway(ip);iptext(ip,v,sizeof(v));s[0]=0;append(s,"Gateway: ",sizeof(s));append(s,v,sizeof(s));gfx_text(bx+24,by+166,s,0x5C7184,1);char n[24];s[0]=0;append(s,"Packets RX/TX: ",sizeof(s));u64dec(net_rx_packets(),n);append(s,n,sizeof(s));append(s," / ",sizeof(s));u64dec(net_tx_packets(),n);append(s,n,sizeof(s));gfx_text(bx+24,by+200,s,0x5C7184,1);gfx_rect(bx+24,by+248,170,36,0x2D6FA8);gfx_text(bx+45,by+250,"Ping gateway",0xFFFFFF,1);if(net_ping_result>=0)gfx_text(bx+220,by+250,net_ping_result?"Reply received":"No reply",net_ping_result?0x2A7B4F:0xA04455,1);gfx_text(bx+24,by+310,"Ethernet / ARP / IPv4 / ICMP / UDP / DNS / TCP / HTTP",0x667C90,1);}
static void draw_about(Window*w){int bx=w->x+1,by=w->y+TITLE_H+1,bw=w->w-2,bh=w->h-TITLE_H-2;gfx_rect(bx,by,bw,bh,0xF0F4F8);gfx_display_text(bx+24,by+22,"ORION",0x1E4267);gfx_text(bx+28,by+93,"UN_Orion Desktop " VERSION,0x273A4D,1);gfx_text(bx+28,by+126,"Kernel architecture: " ORION_ARCH_NAME,0x63768A,1);gfx_text(bx+28,by+162,"Browser: UN_Vela " VELA_VERSION,0x63768A,1);gfx_text(bx+28,by+195,"Engine: Aster " ASTER_VERSION,0x63768A,1);char s[64]="Free physical memory: ",n[24];u64dec(pmm_free_pages()/256,n);append(s,n,sizeof(s));append(s," MB",sizeof(s));gfx_text(bx+28,by+228,s,0x63768A,1);gfx_text(bx+28,by+261,"Traf Typeface v2.1",0x63768A,1);}
static void draw_window_content(int app){Window*w=&wins[app];if(app==APP_TERMINAL)draw_terminal(w);else if(app==APP_FILES)draw_files(w);else if(app==APP_NOTES)draw_notes(w);else if(app==APP_PAINT)draw_paint(w);else if(app==APP_BROWSER)draw_browser(w);else if(app==APP_NETWORK)draw_network(w);else draw_about(w);}
static void draw_windows(void){for(int z=0;z<=ztop;z++)for(int i=0;i<APP_COUNT;i++)if(wins[i].open&&!wins[i].minimized&&wins[i].z==z){draw_window_frame(i);draw_window_content(i);}}
static void redraw(void){cursor_restore();draw_wallpaper();draw_desktop_icons();draw_windows();draw_start_menu();draw_taskbar();cursor_refresh();}

static void focus_app(int app){if(app<0||app>=APP_COUNT)return;wins[app].open=1;wins[app].minimized=0;wins[app].z=++ztop;active_app=app;start_open=0;redraw();}
static int top_window_at(int x,int y){int best=-1,bz=-1;for(int i=0;i<APP_COUNT;i++)if(wins[i].open&&!wins[i].minimized&&inside(x,y,wins[i].x,wins[i].y,wins[i].w,wins[i].h)&&wins[i].z>bz){best=i;bz=wins[i].z;}return best;}
static void paint_at(int x,int y){Window*w=&wins[APP_PAINT];int bx=w->x+1,by=w->y+TITLE_H+1,bw=w->w-2,bh=w->h-TITLE_H-2;int cx=bx+18,cy=by+60,cw=bw-36,ch=bh-78;if(cw>PAINT_W)cw=PAINT_W;if(ch>PAINT_H)ch=PAINT_H;if(!inside(x,y,cx,cy,cw,ch))return;int px=x-cx,py=y-cy;for(int yy=-3;yy<=3;yy++)for(int xx=-3;xx<=3;xx++)if(xx*xx+yy*yy<=9){int nx=px+xx,ny=py+yy;if(nx>=0&&ny>=0&&nx<cw&&ny<ch){paint[ny*PAINT_W+nx]=1;gfx_pixel((uint32_t)(cx+nx),(uint32_t)(cy+ny),0x253A52);}}}
static void handle_press(void){
    uint32_t H=gfx_height();int ty=(int)H-TASKBAR_H;
    if(inside(mouse_x,mouse_y,14,ty+9,38,36)){start_open=!start_open;redraw();return;}
    if(start_open){int sx=14,sy=ty-438;for(int i=0;i<APP_COUNT;i++)if(inside(mouse_x,mouse_y,sx+10,sy+68+i*43,260,36)){focus_app(i);return;}if(inside(mouse_x,mouse_y,sx+10,sy+382,260,36)){gfx_rect(0,0,gfx_width(),gfx_height(),0x05080B);gfx_display_text(80,90,"GOODBYE",0xDCE7F2);__asm__ volatile("cli");for(;;)__asm__ volatile("hlt");}start_open=0;redraw();return;}
    for(int i=0;i<APP_COUNT;i++){int tx=70+i*48;if(inside(mouse_x,mouse_y,tx,ty+8,40,38)){if(wins[i].open&&active_app==i&&!wins[i].minimized){wins[i].minimized=1;active_app=-1;redraw();}else focus_app(i);return;}}
    int icon_apps[6]={APP_TERMINAL,APP_FILES,APP_NOTES,APP_PAINT,APP_BROWSER,APP_NETWORK};for(int i=0;i<6;i++)if(inside(mouse_x,mouse_y,20,36+i*91,88,80)){focus_app(icon_apps[i]);return;}
    int app=top_window_at(mouse_x,mouse_y);if(app>=0){Window*w=&wins[app];w->z=++ztop;active_app=app;if(inside(mouse_x,mouse_y,w->x+w->w-40,w->y+5,34,31)){w->open=0;active_app=-1;redraw();return;}if(inside(mouse_x,mouse_y,w->x,w->y,w->w,TITLE_H)){dragging=app;drag_dx=mouse_x-w->x;drag_dy=mouse_y-w->y;redraw();return;}if(app==APP_PAINT){int bx=w->x+1,by=w->y+TITLE_H+1;if(inside(mouse_x,mouse_y,bx+96,by+8,60,30)){for(int i=0;i<PAINT_W*PAINT_H;i++)paint[i]=0;redraw();return;}cursor_restore();paint_at(mouse_x,mouse_y);cursor_refresh();return;}if(app==APP_BROWSER){int bx=w->x+1,by=w->y+TITLE_H+1,bw=w->w-2,bh=w->h-TITLE_H-2;if(inside(mouse_x,mouse_y,bx+12,by+10,34,34)){if(vela_can_back())vela_back();redraw();return;}if(inside(mouse_x,mouse_y,bx+52,by+10,34,34)){if(vela_can_forward())vela_forward();redraw();return;}if(inside(mouse_x,mouse_y,bx+92,by+10,42,34)){vela_reload();redraw();return;}if(inside(mouse_x,mouse_y,bx+bw-84,by+10,70,34)){vela_go();redraw();return;}if(inside(mouse_x,mouse_y,bx+18,by+108,bw-36,bh-124)){vela_activate_link(mouse_x-(bx+18),mouse_y-(by+108));redraw();return;}}if(app==APP_NETWORK){int bx=w->x+1,by=w->y+TITLE_H+1;if(inside(mouse_x,mouse_y,bx+24,by+248,170,36)){net_ping_result=net_ping_gateway(1800);redraw();return;}}redraw();return;}
    active_app=-1;redraw();
}
static void paint_line(int x0,int y0,int x1,int y1){int dx=x1>x0?x1-x0:x0-x1,sx=x0<x1?1:-1;int dy=-(y1>y0?y1-y0:y0-y1),sy=y0<y1?1:-1;int err=dx+dy;for(;;){paint_at(x0,y0);if(x0==x1&&y0==y1)break;int e2=err*2;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}}
static void handle_mouse_packet(void){
    uint8_t b0=mpkt[0];int dx=(int8_t)mpkt[1],dy=(int8_t)mpkt[2],wheel=0;if(mouse_packet_size()==4){wheel=mpkt[3]&15;if(wheel&8)wheel-=16;}int oldx=mouse_x,oldy=mouse_y;mouse_prev_left=mouse_left;mouse_left=b0&1;cursor_restore();mouse_x+=dx;mouse_y-=dy;int maxx=(int)gfx_width()-16,maxy=(int)gfx_height()-24;if(mouse_x<0)mouse_x=0;if(mouse_y<0)mouse_y=0;if(mouse_x>maxx)mouse_x=maxx;if(mouse_y>maxy)mouse_y=maxy;
    if(wheel&&wins[APP_BROWSER].open&&!wins[APP_BROWSER].minimized){Window*b=&wins[APP_BROWSER];int bx=b->x+1,by=b->y+TITLE_H+1,bw=b->w-2,bh=b->h-TITLE_H-2;if(inside(mouse_x,mouse_y,bx+18,by+108,bw-36,bh-124)){vela_scroll_by(-wheel*48);redraw();return;}}
    int needs_redraw=0;if(dragging>=0&&mouse_left){Window*w=&wins[dragging];w->x=mouse_x-drag_dx;w->y=mouse_y-drag_dy;if(w->x<0)w->x=0;if(w->y<0)w->y=0;if(w->x+w->w>(int)gfx_width())w->x=(int)gfx_width()-w->w;if(w->y+w->h>(int)gfx_height()-TASKBAR_H)w->y=(int)gfx_height()-TASKBAR_H-w->h;needs_redraw=1;}
    if(mouse_prev_left&&!mouse_left)dragging=-1;if(!mouse_prev_left&&mouse_left){cursor_refresh();handle_press();return;}
    if(mouse_left&&mouse_prev_left&&active_app==APP_PAINT&&dragging<0)paint_line(oldx,oldy,mouse_x,mouse_y);if(needs_redraw){draw_wallpaper();draw_desktop_icons();draw_windows();draw_start_menu();draw_taskbar();}cursor_refresh();
}

static const char normal_map[128]={[0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',[0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',[0x0C]='-',[0x0D]='=',[0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',[0x1A]='[',[0x1B]=']',[0x1E]='a',[0x1F]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',[0x24]='j',[0x25]='k',[0x26]='l',[0x27]=';',[0x28]='\'',[0x29]='`',[0x2B]='\\',[0x2C]='z',[0x2D]='x',[0x2E]='c',[0x2F]='v',[0x30]='b',[0x31]='n',[0x32]='m',[0x33]=',',[0x34]='.',[0x35]='/',[0x39]=' '};
static const char shift_map[128]={[0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',[0x07]='^',[0x08]='&',[0x09]='*',[0x0A]='(',[0x0B]=')',[0x0C]='_',[0x0D]='+',[0x10]='Q',[0x11]='W',[0x12]='E',[0x13]='R',[0x14]='T',[0x15]='Y',[0x16]='U',[0x17]='I',[0x18]='O',[0x19]='P',[0x1A]='{',[0x1B]='}',[0x1E]='A',[0x1F]='S',[0x20]='D',[0x21]='F',[0x22]='G',[0x23]='H',[0x24]='J',[0x25]='K',[0x26]='L',[0x27]=':',[0x28]='"',[0x29]='~',[0x2B]='|',[0x2C]='Z',[0x2D]='X',[0x2E]='C',[0x2F]='V',[0x30]='B',[0x31]='N',[0x32]='M',[0x33]='<',[0x34]='>',[0x35]='?',[0x39]=' '};
static void terminal_exec(void){serial_write("desktop shell: ");serial_write(term_cmd);serial_write("\r\n");if(term_len){char e[TERM_LEN]="orion > ";append(e,term_cmd,sizeof(e));term_push(e);}if(streq(term_cmd,"help"))term_push("help clear info mem alloc uptime desktop net ping vela nettest openwrt");else if(streq(term_cmd,"clear"))term_count=0;else if(streq(term_cmd,"info"))term_push("UN_Orion " VERSION " / " ORION_ARCH_NAME " / UN_Vela " VELA_VERSION " / Aster " ASTER_VERSION);else if(streq(term_cmd,"desktop"))term_push("Terminal, Files, Notes, Paint, UN_Vela and Network are native apps.");else if(streq(term_cmd,"mem")){char s[TERM_LEN]="free physical memory ",n[24];u64dec(pmm_free_pages()/256,n);append(s,n,sizeof(s));append(s," MB",sizeof(s));term_push(s);}else if(streq(term_cmd,"alloc")){uint64_t p=pmm_alloc_page();char s[TERM_LEN]="allocated ",h[19];hex64(p,h);append(s,h,sizeof(s));term_push(s);}else if(streq(term_cmd,"uptime")){char s[TERM_LEN]="uptime ",n[24];u64dec(timer_frequency()?timer_ticks()/timer_frequency():0,n);append(s,n,sizeof(s));append(s," seconds",sizeof(s));term_push(s);}else if(streq(term_cmd,"net")){char s[TERM_LEN]="network ";if(net_ready()){uint8_t ip[4];char v[32];append(s,"connected / ",sizeof(s));append(s,net_driver_name(),sizeof(s));append(s," / ",sizeof(s));net_get_ipv4(ip);iptext(ip,v,sizeof(v));append(s,v,sizeof(s));}else append(s,"offline",sizeof(s));term_push(s);}else if(streq(term_cmd,"ping"))term_push(net_ping_gateway(1500)?"gateway reply received":"gateway no reply");else if(streq(term_cmd,"nettest")){if(vela_load_url("10.0.2.2:18080/")){term_push("HTTP test success / loaded by UN_Vela + Aster");serial_write("HTTP test success\r\n");}else{term_push(vela_status());serial_write("HTTP test failed\r\n");}}else if(streq(term_cmd,"openwrt")){static const uint8_t ip[4]={192,168,1,2},mask[4]={255,255,255,0},gw[4]={192,168,1,1},dns[4]={192,168,1,1};net_configure(ip,mask,gw,dns);term_push("network profile: OpenWrt LAN 192.168.1.2/24");}else if(streq(term_cmd,"vela")||streq(term_cmd,"browser")){focus_app(APP_BROWSER);term_len=0;term_cmd[0]=0;return;}else if(term_len)term_push("unknown command");term_len=0;term_cmd[0]=0;redraw();}
static void key_ascii(char c){if(active_app==APP_TERMINAL){if(term_len<(int)sizeof(term_cmd)-1){term_cmd[term_len++]=c;term_cmd[term_len]=0;redraw();}}else if(active_app==APP_NOTES){if(note_len<NOTE_CAP-1){notes[note_len++]=c;notes[note_len]=0;redraw();}}else if(active_app==APP_BROWSER){vela_input_char(c);redraw();}}
void desktop_key_scancode(uint8_t s){
    if(s==0x2A||s==0x36){shift_down=1;return;}if(s==0xAA||s==0xB6){shift_down=0;return;}if(s&0x80)return;if(s==0x01){if(start_open){start_open=0;redraw();}return;}
    if(s==0x1C){if(active_app==APP_TERMINAL)terminal_exec();else if(active_app==APP_NOTES&&note_len<NOTE_CAP-1){notes[note_len++]='\n';notes[note_len]=0;redraw();}else if(active_app==APP_BROWSER){vela_go();redraw();}return;}
    if(s==0x0E){if(active_app==APP_TERMINAL&&term_len){term_cmd[--term_len]=0;redraw();}else if(active_app==APP_NOTES&&note_len){notes[--note_len]=0;redraw();}else if(active_app==APP_BROWSER){vela_backspace();redraw();}return;}
    char c=s<128?(shift_down?shift_map[s]:normal_map[s]):0;if(c)key_ascii(c);
}
void desktop_mouse_byte(uint8_t b){if(mpkt_i==0&&!(b&0x08))return;int need=mouse_packet_size();if(need!=4)need=3;mpkt[mpkt_i++]=b;if(mpkt_i>=need){mpkt_i=0;handle_mouse_packet();}}
void desktop_tick(void){uint64_t sec=timer_frequency()?timer_ticks()/timer_frequency():0;if(sec!=last_second){last_second=sec;cursor_restore();draw_taskbar();cursor_refresh();}}
void desktop_init(OrionBootInfo *bi){
    g_bi=bi;(void)g_bi;wins[APP_TERMINAL]=(Window){280,130,720,480,1,0,11,"Terminal"};wins[APP_FILES]=(Window){230,115,700,440,0,0,2,"Files"};wins[APP_NOTES]=(Window){330,125,610,480,0,0,3,"Notes"};wins[APP_PAINT]=(Window){250,90,760,560,0,0,4,"Paint"};wins[APP_BROWSER]=(Window){220,85,820,590,0,0,5,"UN_Vela"};wins[APP_NETWORK]=(Window){345,135,620,430,0,0,6,"Network"};wins[APP_ABOUT]=(Window){380,165,540,380,0,0,7,"About UN_Orion"};active_app=APP_TERMINAL;ztop=11;vela_init(wins[APP_BROWSER].w-2);term_push("Welcome to UN_Orion Desktop " VERSION ".");term_push("UN_Vela " VELA_VERSION " uses Aster Engine " ASTER_VERSION ".");term_push("Type 'help' for terminal commands.");redraw();
}
