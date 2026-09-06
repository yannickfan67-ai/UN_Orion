#include <stdint.h>
#include <stddef.h>
#include "bootinfo.h"
#include "io.h"
#include "serial.h"
#include "graphics.h"
#include "interrupts.h"
#include "pmm.h"

#define VERSION "0.0.3"
#define MAX_LINES 12
#define LINE_LEN 78

static OrionBootInfo *g_bi;
static char console_lines[MAX_LINES][LINE_LEN];
static int line_count;
static char cmd[60];
static int cmd_len;
static int shift_down;
static uint32_t console_x,console_y,console_w,console_h;
static char cpu_vendor[13]="UNKNOWN";
static char cpu_brand[49]="UNKNOWN CPU";

static void u64_dec(uint64_t v,char*out){char t[24];int n=0;if(!v){out[0]='0';out[1]=0;return;}while(v){t[n++]=(char)('0'+v%10);v/=10;}for(int i=0;i<n;i++)out[i]=t[n-1-i];out[n]=0;}
static void hex64(uint64_t v,char out[19]){static const char h[]="0123456789ABCDEF";out[0]='0';out[1]='x';for(int i=0;i<16;i++)out[i+2]=h[(v>>(60-4*i))&15];out[18]=0;}
static void append(char*a,const char*b,size_t cap){size_t i=0;while(i<cap&&a[i])i++;while(i+1<cap&&b&&*b)a[i++]=*b++;if(i<cap)a[i]=0;}
static int streq(const char*a,const char*b){while(*a&&*b&&*a==*b){a++;b++;}return *a==*b;}
static void cpuid(uint32_t leaf,uint32_t sub,uint32_t*a,uint32_t*b,uint32_t*c,uint32_t*d){__asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(sub));}
static void cpu_init(void){
    uint32_t a,b,c,d;cpuid(0,0,&a,&b,&c,&d);((uint32_t*)cpu_vendor)[0]=b;((uint32_t*)cpu_vendor)[1]=d;((uint32_t*)cpu_vendor)[2]=c;cpu_vendor[12]=0;
    cpuid(0x80000000U,0,&a,&b,&c,&d);if(a>=0x80000004U){uint32_t *p=(uint32_t*)cpu_brand;for(uint32_t leaf=0x80000002U;leaf<=0x80000004U;leaf++){cpuid(leaf,0,&a,&b,&c,&d);*p++=a;*p++=b;*p++=c;*p++=d;}cpu_brand[48]=0;while(cpu_brand[0]==' '){for(int i=0;i<48;i++)cpu_brand[i]=cpu_brand[i+1];}}
}

typedef struct{uint32_t type;uint32_t pad;uint64_t physical_start;uint64_t virtual_start;uint64_t pages;uint64_t attr;} EfiMemDesc;
static uint64_t uefi_usable_mb(void){if(!g_bi||!g_bi->memory_descriptor_size)return 0;uint64_t pages=0;for(uint64_t off=0;off+sizeof(EfiMemDesc)<=g_bi->memory_map_size;off+=g_bi->memory_descriptor_size){EfiMemDesc*d=(EfiMemDesc*)(uintptr_t)(g_bi->memory_map+off);if(d->type==7)pages+=d->pages;}return pages/256;}

static void push_line(const char*s){
    int row;if(line_count<MAX_LINES)row=line_count++;else{for(int i=0;i<MAX_LINES-1;i++)for(int j=0;j<LINE_LEN;j++)console_lines[i][j]=console_lines[i+1][j];row=MAX_LINES-1;}
    int j=0;while(s&&*s&&j<LINE_LEN-1)console_lines[row][j++]=*s++;console_lines[row][j]=0;
}
static void draw_console(void){
    gfx_rect(console_x,console_y,console_w,console_h,0x101720);gfx_rect(console_x,console_y,console_w,40,0x17212D);
    gfx_text(console_x+20,console_y+7,"ORION CONSOLE",0xDCE7F3,1);gfx_text_right(console_x+console_w-18,console_y+7,"IRQ0+IRQ1 / READY",0x71859A);
    uint32_t y=console_y+51;for(int i=0;i<line_count;i++,y+=23)gfx_text(console_x+20,y,console_lines[i],i==line_count-1?0xBBCBDB:0x8799AC,1);
    gfx_rect(console_x+16,console_y+console_h-42,console_w-32,32,0x0A0F16);gfx_text(console_x+24,console_y+console_h-39,"orion >",0x73B3FF,1);gfx_text(console_x+120,console_y+console_h-39,cmd,0xEDF3F9,1);
}
static void format_uptime(uint64_t sec,char out[16]){
    uint64_t h=(sec/3600)%100,m=(sec/60)%60,s=sec%60;out[0]='U';out[1]='P';out[2]=' ';out[3]=(char)('0'+h/10);out[4]=(char)('0'+h%10);out[5]=':';out[6]=(char)('0'+m/10);out[7]=(char)('0'+m%10);out[8]=':';out[9]=(char)('0'+s/10);out[10]=(char)('0'+s%10);out[11]=0;
}
static void draw_uptime(uint64_t sec){uint32_t W=gfx_width();if(W<700)return;char s[16];format_uptime(sec,s);gfx_rect(W-490,8,190,38,0x0F1620);gfx_text(W-478,11,s,0x8FB2D6,1);}
static void draw_ui(void){
    uint32_t W=gfx_width(),H=gfx_height();gfx_rect(0,0,W,H,0x0A1017);gfx_rect(0,0,W,54,0x0F1620);gfx_rect(0,53,W,1,0x273546);gfx_rect(0,0,5,H,0x62A8FF);
    gfx_text(30,11,"UN ORION",0xECF2F8,1);gfx_text(170,11,"DEVELOPER PREVIEW  " VERSION,0x718397,1);gfx_text_right(W-28,11,"UEFI / X86_64",0x8FA6BD);draw_uptime(0);
    uint32_t side=282;gfx_rect(20,78,side,H-98,0x0F161F);gfx_text(42,98,"SYSTEM",0x76AEF5,1);gfx_line_h(42,133,side-44,0x283746);
    gfx_text(42,151,"STATUS",0x64778B,1);gfx_text(42,179,"READY",0xE1EAF4,1);
    gfx_text(42,216,"CPU",0x64778B,1);gfx_text(42,244,cpu_vendor,0xE1EAF4,1);
    gfx_text(42,281,"MEMORY",0x64778B,1);char n[32];u64_dec(uefi_usable_mb(),n);append(n," MB",sizeof(n));gfx_text(42,309,n,0xE1EAF4,1);
    gfx_text(42,346,"PMM FREE",0x64778B,1);u64_dec(pmm_free_pages()/256,n);append(n," MB",sizeof(n));gfx_text(42,374,n,0xE1EAF4,1);
    gfx_text(42,411,"DISPLAY",0x64778B,1);char disp[40],x[16];disp[0]=0;u64_dec(W,x);append(disp,x,sizeof(disp));append(disp," X ",sizeof(disp));u64_dec(H,x);append(disp,x,sizeof(disp));gfx_text(42,439,disp,0xE1EAF4,1);
    gfx_text(42,476,"TIMER",0x64778B,1);u64_dec(timer_frequency(),n);append(n," HZ PIT",sizeof(n));gfx_text(42,504,n,0xE1EAF4,1);
    gfx_text(42,H-146,"KERNEL",0x64778B,1);gfx_text(42,H-118,"FROM SCRATCH",0xE1EAF4,1);gfx_text(42,H-80,"TRAF TYPEFACE",0x64778B,1);gfx_text(42,H-52,"V2.1 / 2.100",0xE1EAF4,1);
    uint32_t mx=side+50,mw=W-mx-24;gfx_rect(mx,78,mw,220,0x111B27);gfx_rect(mx,78,4,220,0x62A8FF);gfx_display_text(mx+34,98,"ORION",0xF3F7FB);gfx_text(mx+38,171,"A SMALL SYSTEM, BUILT FROM THE METAL UP.",0x92A8BE,1);gfx_line_h(mx+38,214,mw-76,0x2A3B4D);
    const char*labels[]={"GDT","IDT","PIC","PIT","PMM","GOP","COM1"};for(int i=0;i<7;i++){uint32_t px=mx+38+(uint32_t)i*75;gfx_text(px,230,labels[i],0x64778B,1);gfx_text(px,255,"ON",0x7FCC9B,1);}
    console_x=mx;console_y=320;console_w=mw;console_h=H-console_y-20;draw_console();
}

static void shell_exec(void){
    serial_write("shell cmd: ");serial_write(cmd);serial_write("\r\n");
    if(cmd_len){char echo[LINE_LEN]="orion > ";append(echo,cmd,sizeof(echo));push_line(echo);}
    if(streq(cmd,"help")){push_line("help clear info cpu mem pmm alloc uptime");push_line("reboot halt fault");}
    else if(streq(cmd,"info")){push_line("UN_Orion " VERSION " / x86_64 / UEFI GOP / Traf Typeface v2.1");}
    else if(streq(cmd,"cpu")){char s[LINE_LEN]="CPU: ";append(s,cpu_brand,sizeof(s));push_line(s);}
    else if(streq(cmd,"mem")){char s[LINE_LEN]="UEFI conventional memory: ",n[24];u64_dec(uefi_usable_mb(),n);append(s,n,sizeof(s));append(s," MB",sizeof(s));push_line(s);}
    else if(streq(cmd,"pmm")){char s[LINE_LEN]="PMM pages total=",n[24];u64_dec(pmm_total_pages(),n);append(s,n,sizeof(s));append(s," free=",sizeof(s));u64_dec(pmm_free_pages(),n);append(s,n,sizeof(s));append(s," used=",sizeof(s));u64_dec(pmm_used_pages(),n);append(s,n,sizeof(s));push_line(s);}
    else if(streq(cmd,"alloc")){uint64_t p=pmm_alloc_page();char s[LINE_LEN];if(p){char h[19];hex64(p,h);s[0]=0;append(s,"allocated physical page: ",sizeof(s));append(s,h,sizeof(s));}else{for(int i=0;i<LINE_LEN;i++)s[i]=0;append(s,"PMM exhausted",sizeof(s));}push_line(s);}
    else if(streq(cmd,"uptime")){char s[LINE_LEN]="uptime ",n[24];uint32_t hz=timer_frequency();uint64_t ticks=timer_ticks();u64_dec(hz?ticks/hz:0,n);append(s,n,sizeof(s));append(s," s / ticks ",sizeof(s));u64_dec(ticks,n);append(s,n,sizeof(s));push_line(s);}
    else if(streq(cmd,"clear")){line_count=0;}
    else if(streq(cmd,"reboot")){push_line("rebooting...");draw_console();while(inb(0x64)&2){}outb(0x64,0xFE);}
    else if(streq(cmd,"halt")){push_line("halted");draw_console();__asm__ volatile("cli");for(;;)__asm__ volatile("hlt");}
    else if(streq(cmd,"fault")){push_line("triggering #UD exception for panic-path test...");draw_console();serial_write("fault test requested\r\n");__asm__ volatile("ud2");}
    else if(cmd_len){push_line("unknown command. type 'help'.");}
    cmd_len=0;cmd[0]=0;draw_console();
}

static const char normal_map[128]={[0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',[0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',[0x0C]='-',[0x0D]='=',[0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',[0x1A]='[',[0x1B]=']',[0x1E]='a',[0x1F]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',[0x24]='j',[0x25]='k',[0x26]='l',[0x27]=';',[0x28]='\'',[0x29]='`',[0x2B]='\\',[0x2C]='z',[0x2D]='x',[0x2E]='c',[0x2F]='v',[0x30]='b',[0x31]='n',[0x32]='m',[0x33]=',',[0x34]='.',[0x35]='/',[0x39]=' '};
static const char shift_map[128]={[0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',[0x07]='^',[0x08]='&',[0x09]='*',[0x0A]='(',[0x0B]=')',[0x0C]='_',[0x0D]='+',[0x10]='Q',[0x11]='W',[0x12]='E',[0x13]='R',[0x14]='T',[0x15]='Y',[0x16]='U',[0x17]='I',[0x18]='O',[0x19]='P',[0x1A]='{',[0x1B]='}',[0x1E]='A',[0x1F]='S',[0x20]='D',[0x21]='F',[0x22]='G',[0x23]='H',[0x24]='J',[0x25]='K',[0x26]='L',[0x27]=':',[0x28]='"',[0x29]='~',[0x2B]='|',[0x2C]='Z',[0x2D]='X',[0x2E]='C',[0x2F]='V',[0x30]='B',[0x31]='N',[0x32]='M',[0x33]='<',[0x34]='>',[0x35]='?',[0x39]=' '};
static void handle_scan(uint8_t s){
    if(s==0x2A||s==0x36){shift_down=1;return;}if(s==0xAA||s==0xB6){shift_down=0;return;}if(s&0x80)return;
    if(s==0x1C){shell_exec();return;}if(s==0x0E){if(cmd_len){cmd[--cmd_len]=0;draw_console();}return;}
    char c=(s<128?(shift_down?shift_map[s]:normal_map[s]):0);if(c&&cmd_len<(int)sizeof(cmd)-1){cmd[cmd_len++]=c;cmd[cmd_len]=0;draw_console();}
}

__attribute__((noreturn)) void kernel_main(OrionBootInfo *bi){
    __asm__ volatile("cli");g_bi=bi;serial_init();gfx_init(bi);cpu_init();
    serial_write("UN_Orion kernel 0.0.3 alive\r\n");
    arch_gdt_init();serial_write("GDT ready\r\n");
    pmm_init(bi);serial_write("PMM ready\r\n");
    interrupts_init(100);serial_write("IDT/PIC/PIT/keyboard ready\r\n");
    if(gfx_ready()){push_line("UN_Orion 0.0.3 kernel ready.");push_line("PIT clock + PMM + CPU exception path active.");push_line("Type 'help' to list commands.");draw_ui();}
    __asm__ volatile("sti");uint64_t last_sec=(uint64_t)-1;
    for(;;){uint8_t s;while(keyboard_pop_scancode(&s))handle_scan(s);uint32_t hz=timer_frequency();uint64_t sec=hz?timer_ticks()/hz:0;if(sec!=last_sec){last_sec=sec;draw_uptime(sec);}__asm__ volatile("sti; hlt");}
}
