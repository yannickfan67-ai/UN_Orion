#include <stdint.h>
#include <stddef.h>
#include "bootinfo.h"
#include "traf_font_22.h"
#include "traf_display_44.h"

#define VERSION "0.0.2"
#define COM1 0x3F8
#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_DATA 0x21
#define PIC2_DATA 0xA1

static OrionBootInfo *g_bi;
static uint8_t keyq[64];
static volatile uint8_t key_head, key_tail;
static int shift_down;

static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static void io_wait(void){outb(0x80,0);}
static void serial_init(void){outb(COM1+1,0);outb(COM1+3,0x80);outb(COM1,3);outb(COM1+1,0);outb(COM1+3,3);outb(COM1+2,0xC7);outb(COM1+4,0x0B);}
static void serial_putc(char c){while(!(inb(COM1+5)&0x20)){}outb(COM1,(uint8_t)c);}
static void serial_write(const char*s){while(*s)serial_putc(*s++);}

static uint32_t pack_pixel(const OrionBootInfo*b,uint32_t rgb){uint32_t r=(rgb>>16)&255,g=(rgb>>8)&255,bl=rgb&255;return b->pixel_format==0?(bl<<16)|(g<<8)|r:(r<<16)|(g<<8)|bl;}
static void rect(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t c){if(!g_bi)return;if(x>=g_bi->width||y>=g_bi->height)return;if(x+w>g_bi->width)w=g_bi->width-x;if(y+h>g_bi->height)h=g_bi->height-y;for(uint32_t yy=0;yy<h;yy++){volatile uint32_t*fb=(volatile uint32_t*)(uintptr_t)g_bi->framebuffer_base+(uint64_t)(y+yy)*g_bi->pixels_per_scanline+x;uint32_t pc=pack_pixel(g_bi,c);for(uint32_t xx=0;xx<w;xx++)fb[xx]=pc;}}
static void line_h(uint32_t x,uint32_t y,uint32_t w,uint32_t c){rect(x,y,w,1,c);}

static uint32_t draw_char(uint32_t x,uint32_t y,char ch,uint32_t color,uint32_t scale){if(ch<TRAF_FIRST||ch>TRAF_LAST)ch='?';unsigned gi=(unsigned)ch-TRAF_FIRST;for(uint32_t yy=0;yy<TRAF_H;yy++){uint32_t bits=traf_rows[gi][yy];for(uint32_t xx=0;xx<TRAF_W;xx++)if(bits&(1u<<(TRAF_W-1-xx)))rect(x+xx*scale,y+yy*scale,scale,scale,color);}return (traf_advance[gi]+1)*scale;}
static uint32_t text(uint32_t x,uint32_t y,const char*s,uint32_t c,uint32_t scale){uint32_t ox=x;while(*s)x+=draw_char(x,y,*s++,c,scale);return x-ox;}
static uint32_t display_char(uint32_t x,uint32_t y,char ch,uint32_t color){if(ch<'A'||ch>'Z')return draw_char(x,y,ch,color,1);unsigned gi=(unsigned)(ch-'A');for(uint32_t yy=0;yy<TRAF_D_H;yy++){uint64_t bits=traf_d_rows[gi][yy];for(uint32_t xx=0;xx<TRAF_D_W;xx++)if(bits&(1ULL<<(TRAF_D_W-1-xx)))rect(x+xx,y+yy,1,1,color);}return traf_d_advance[gi]+2;}
static uint32_t display_text(uint32_t x,uint32_t y,const char*s,uint32_t c){uint32_t ox=x;while(*s)x+=display_char(x,y,*s++,c);return x-ox;}
static void text_right(uint32_t right,uint32_t y,const char*s,uint32_t c){uint32_t w=0;for(const char*p=s;*p;p++){char ch=*p;if(ch<32||ch>126)ch='?';w+=traf_advance[(unsigned)ch-32]+1;}text(right>w?right-w:0,y,s,c,1);}

static void u64_dec(uint64_t v,char*out){char tmp[24];int n=0;if(!v){out[0]='0';out[1]=0;return;}while(v){tmp[n++]=(char)('0'+v%10);v/=10;}for(int i=0;i<n;i++)out[i]=tmp[n-1-i];out[n]=0;}
static void append(char*a,const char*b,size_t cap){size_t i=0;while(i<cap&&a[i])i++;while(i+1<cap&&*b)a[i++]=*b++;if(i<cap)a[i]=0;}
static int streq(const char*a,const char*b){while(*a&&*b&&*a==*b){a++;b++;}return *a==*b;}

typedef struct{uint32_t type;uint32_t pad;uint64_t physical_start;uint64_t virtual_start;uint64_t pages;uint64_t attr;} EfiMemDesc;
static uint64_t usable_mb(void){if(!g_bi||!g_bi->memory_descriptor_size)return 0;uint64_t pages=0;for(uint64_t off=0;off+sizeof(EfiMemDesc)<=g_bi->memory_map_size;off+=g_bi->memory_descriptor_size){EfiMemDesc*d=(EfiMemDesc*)(uintptr_t)(g_bi->memory_map+off);if(d->type==1||d->type==2||d->type==3||d->type==4||d->type==7)pages+=d->pages;}return pages/256;}

struct __attribute__((packed)) GdtPtr{uint16_t limit;uint64_t base;};
static uint64_t gdt[3]={0,0x00AF9A000000FFFFULL,0x00CF92000000FFFFULL};
extern void orion_load_gdt(struct GdtPtr*);
static void gdt_init(void){struct GdtPtr p={sizeof(gdt)-1,(uint64_t)(uintptr_t)gdt};orion_load_gdt(&p);}

typedef struct __attribute__((packed)){uint16_t off0;uint16_t sel;uint8_t ist;uint8_t attr;uint16_t off1;uint32_t off2;uint32_t zero;} IdtGate;
struct __attribute__((packed)) IdtPtr{uint16_t limit;uint64_t base;};
static IdtGate idt[256];
struct IntFrame{uint64_t rip,cs,rflags,rsp,ss;};
__attribute__((interrupt)) static void keyboard_isr(struct IntFrame*f){(void)f;uint8_t s=inb(0x60);uint8_t next=(uint8_t)(key_head+1)&63;if(next!=key_tail){keyq[key_head]=s;key_head=next;}outb(PIC1,0x20);}
static void idt_gate(int v,void*fn){uint64_t a=(uint64_t)(uintptr_t)fn;idt[v]=(IdtGate){a&0xffff,0x08,0,0x8e,(a>>16)&0xffff,(uint32_t)(a>>32),0};}
static void pic_init(void){uint8_t m1=inb(PIC1_DATA),m2=inb(PIC2_DATA);outb(PIC1,0x11);io_wait();outb(PIC2,0x11);io_wait();outb(PIC1_DATA,0x20);io_wait();outb(PIC2_DATA,0x28);io_wait();outb(PIC1_DATA,4);io_wait();outb(PIC2_DATA,2);io_wait();outb(PIC1_DATA,1);io_wait();outb(PIC2_DATA,1);io_wait();(void)m1;(void)m2;outb(PIC1_DATA,0xFD);outb(PIC2_DATA,0xFF);}
static void idt_init(void){for(int i=0;i<256;i++)idt[i]=(IdtGate){0};idt_gate(33,(void*)keyboard_isr);struct IdtPtr p={sizeof(idt)-1,(uint64_t)(uintptr_t)idt};__asm__ volatile("lidt %0"::"m"(p));pic_init();}

#define MAX_LINES 11
#define LINE_LEN 76
static char console_lines[MAX_LINES][LINE_LEN];
static int line_count;
static char cmd[56];static int cmd_len;
static uint32_t console_x,console_y,console_w,console_h;
static void push_line(const char*s){int row;if(line_count<MAX_LINES){row=line_count++;}else{for(int i=0;i<MAX_LINES-1;i++)for(int j=0;j<LINE_LEN;j++)console_lines[i][j]=console_lines[i+1][j];row=MAX_LINES-1;}int j=0;while(s&&*s&&j<LINE_LEN-1)console_lines[row][j++]=*s++;console_lines[row][j]=0;}
static void draw_console(void){rect(console_x,console_y,console_w,console_h,0x11161E);rect(console_x,console_y,console_w,40,0x171E28);text(console_x+20,console_y+7,"ORION CONSOLE",0xDCE6F2,1);text_right(console_x+console_w-18,console_y+7,"IRQ KEYBOARD / READY",0x718096);uint32_t y=console_y+54;for(int i=0;i<line_count;i++,y+=24)text(console_x+20,y,console_lines[i],i==line_count-1?0xB9C8DA:0x8FA1B8,1);rect(console_x+16,console_y+console_h-42,console_w-32,32,0x0B0F15);text(console_x+24,console_y+console_h-39,"orion >",0x79B8FF,1);text(console_x+120,console_y+console_h-39,cmd,0xE8EEF6,1);}
static void draw_ui(void){uint32_t W=g_bi->width,H=g_bi->height;rect(0,0,W,H,0x0B1017);rect(0,0,W,54,0x0F1620);rect(0,53,W,1,0x273444);rect(0,0,5,H,0x62A8FF);text(30,11,"UN ORION",0xE8EEF6,1);text(170,11,"DEVELOPER PREVIEW  " VERSION,0x718096,1);text_right(W-28,11,"UEFI / X86_64",0x8EA3BA);
uint32_t side=280;rect(20,78,side,H-98,0x0F151E);text(42,98,"SYSTEM",0x7AAFFF,1);line_h(42,133,side-44,0x263444);text(42,154,"STATUS",0x65778C,1);text(42,182,"READY",0xDDE8F4,1);text(42,226,"ARCH",0x65778C,1);text(42,254,"X86_64",0xDDE8F4,1);text(42,298,"MEMORY",0x65778C,1);char mb[32];u64_dec(usable_mb(),mb);append(mb," MB USABLE",sizeof(mb));text(42,326,mb,0xDDE8F4,1);text(42,370,"DISPLAY",0x65778C,1);char disp[40],n[16];disp[0]=0;u64_dec(W,n);append(disp,n,sizeof(disp));append(disp," X ",sizeof(disp));u64_dec(H,n);append(disp,n,sizeof(disp));text(42,398,disp,0xDDE8F4,1);text(42,H-150,"KERNEL",0x65778C,1);text(42,H-122,"FROM SCRATCH",0xDDE8F4,1);text(42,H-82,"TRAF TYPEFACE",0x65778C,1);text(42,H-54,"V2.1 / 2.100",0xDDE8F4,1);
uint32_t mx=side+48;uint32_t mw=W-mx-24;rect(mx,78,mw,210,0x111A26);rect(mx,78,4,210,0x62A8FF);display_text(mx+34,100,"ORION",0xF0F5FA);text(mx+38,173,"A SMALL SYSTEM, BUILT FROM THE METAL UP.",0x91A6BC,1);line_h(mx+38,217,mw-76,0x2A3A4C);text(mx+38,235,"GDT",0x65778C,1);text(mx+116,235,"IDT",0x65778C,1);text(mx+188,235,"IRQ1",0x65778C,1);text(mx+270,235,"GOP",0x65778C,1);text(mx+345,235,"COM1",0x65778C,1);text(mx+38,257,"ON",0x80C99B,1);text(mx+116,257,"ON",0x80C99B,1);text(mx+188,257,"ON",0x80C99B,1);text(mx+270,257,"ON",0x80C99B,1);text(mx+345,257,"ON",0x80C99B,1);
console_x=mx;console_y=310;console_w=mw;console_h=H-console_y-20;draw_console();}
static void shell_exec(void){serial_write("shell cmd: ");serial_write(cmd);serial_write("\r\n");char echo[LINE_LEN]="orion > ";append(echo,cmd,sizeof(echo));push_line(echo);if(streq(cmd,"help")){push_line("help  clear  info  mem  reboot  halt");}else if(streq(cmd,"info")){push_line("UN_Orion " VERSION " / x86_64 / UEFI GOP / Traf Typeface v2.1");}else if(streq(cmd,"mem")){char s[LINE_LEN]="usable memory: ",n[24];u64_dec(usable_mb(),n);append(s,n,sizeof(s));append(s," MB",sizeof(s));push_line(s);}else if(streq(cmd,"clear")){line_count=0;}else if(streq(cmd,"reboot")){push_line("rebooting...");draw_console();while(inb(0x64)&2){}outb(0x64,0xFE);}else if(streq(cmd,"halt")){push_line("halted");draw_console();__asm__ volatile("cli");for(;;)__asm__ volatile("hlt");}else if(cmd_len){push_line("unknown command. type 'help'.");}cmd_len=0;cmd[0]=0;draw_console();}
static const char normal_map[128]={[0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',[0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',[0x0C]='-',[0x0D]='=',[0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',[0x1A]='[',[0x1B]=']',[0x1E]='a',[0x1F]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',[0x24]='j',[0x25]='k',[0x26]='l',[0x27]=';',[0x28]='\'',[0x29]='`',[0x2B]='\\',[0x2C]='z',[0x2D]='x',[0x2E]='c',[0x2F]='v',[0x30]='b',[0x31]='n',[0x32]='m',[0x33]=',',[0x34]='.',[0x35]='/',[0x39]=' '};
static const char shift_map[128]={[0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',[0x07]='^',[0x08]='&',[0x09]='*',[0x0A]='(',[0x0B]=')',[0x0C]='_',[0x0D]='+',[0x10]='Q',[0x11]='W',[0x12]='E',[0x13]='R',[0x14]='T',[0x15]='Y',[0x16]='U',[0x17]='I',[0x18]='O',[0x19]='P',[0x1A]='{',[0x1B]='}',[0x1E]='A',[0x1F]='S',[0x20]='D',[0x21]='F',[0x22]='G',[0x23]='H',[0x24]='J',[0x25]='K',[0x26]='L',[0x27]=':',[0x28]='"',[0x29]='~',[0x2B]='|',[0x2C]='Z',[0x2D]='X',[0x2E]='C',[0x2F]='V',[0x30]='B',[0x31]='N',[0x32]='M',[0x33]='<',[0x34]='>',[0x35]='?',[0x39]=' '};
static void handle_scan(uint8_t s){if(s==0x2A||s==0x36){shift_down=1;return;}if(s==0xAA||s==0xB6){shift_down=0;return;}if(s&0x80)return;if(s==0x1C){shell_exec();return;}if(s==0x0E){if(cmd_len){cmd[--cmd_len]=0;draw_console();}return;}char c=(s<128?(shift_down?shift_map[s]:normal_map[s]):0);if(c&&cmd_len<(int)sizeof(cmd)-1){cmd[cmd_len++]=c;cmd[cmd_len]=0;draw_console();}}

__attribute__((noreturn)) void kernel_main(OrionBootInfo*bi){__asm__ volatile("cli");g_bi=bi;serial_init();serial_write("UN_Orion kernel 0.0.2 alive\r\n");gdt_init();serial_write("GDT ready\r\n");idt_init();serial_write("IDT/PIC/keyboard ready\r\n");if(g_bi&&g_bi->framebuffer_base){push_line("UN_Orion kernel ready.");push_line("Type 'help' to list commands.");draw_ui();}__asm__ volatile("sti");for(;;){while(key_tail!=key_head){uint8_t s=keyq[key_tail];key_tail=(uint8_t)(key_tail+1)&63;handle_scan(s);}__asm__ volatile("sti; hlt");}}
