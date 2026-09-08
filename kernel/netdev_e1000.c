#include <stdint.h>
#include <stddef.h>
#include "pci.h"
#include "netdev_internal.h"
#define N 32
#define BUF 2048
#define REG_CTRL 0x0000
#define REG_ICR 0x00c0
#define REG_IMC 0x00d8
#define REG_RCTL 0x0100
#define REG_TCTL 0x0400
#define REG_TIPG 0x0410
#define REG_RDBAL 0x2800
#define REG_RDBAH 0x2804
#define REG_RDLEN 0x2808
#define REG_RDH 0x2810
#define REG_RDT 0x2818
#define REG_TDBAL 0x3800
#define REG_TDBAH 0x3804
#define REG_TDLEN 0x3808
#define REG_TDH 0x3810
#define REG_TDT 0x3818
#define REG_RAL 0x5400
#define REG_RAH 0x5404
#define RCTL_EN (1u<<1)
#define RCTL_BAM (1u<<15)
#define RCTL_SECRC (1u<<26)
#define TCTL_EN (1u<<1)
#define TCTL_PSP (1u<<3)
typedef struct __attribute__((packed)){uint64_t addr;uint16_t len,csum;uint8_t status,errors;uint16_t special;} RxD;
typedef struct __attribute__((packed)){uint64_t addr;uint16_t len;uint8_t cso,cmd,status,css;uint16_t special;} TxD;
static volatile uint8_t *mmio;static RxD rx[N] __attribute__((aligned(16)));static TxD txd[N] __attribute__((aligned(16)));static uint8_t rxb[N][BUF] __attribute__((aligned(16)));static uint8_t txb[N][BUF] __attribute__((aligned(16)));static unsigned rxi,txi;static int up;static const char *model="Intel e1000";
static void cp(void*d,const void*s,size_t n){uint8_t*a=d;const uint8_t*b=s;while(n--)*a++=*b++;}
static void zero(void*d,size_t n){uint8_t*p=d;while(n--)*p++=0;}
static uint32_t rd(unsigned o){return *(volatile uint32_t*)(mmio+o);}static void wr(unsigned o,uint32_t v){*(volatile uint32_t*)(mmio+o)=v;}
static int find(PciDevice *d){static const struct{uint16_t id;const char*n;} ids[]={{0x100e,"Intel PRO/1000 MT Desktop (82540EM)"},{0x1004,"Intel PRO/1000 T Server (82543GC)"},{0x100f,"Intel PRO/1000 MT Server (82545EM)"},{0x150c,"Intel 82583V Gigabit Network Connection"},{0x10d3,"Intel e1000e family (82574L test path)"}};for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);i++)if(pci_find(0x8086,ids[i].id,d)){model=ids[i].n;return 1;}return 0;}
static int init(uint8_t mac[6]){PciDevice d;if(!find(&d))return 0;uint32_t bar=pci_bar32(&d,0);if(bar&1)return 0;uint64_t m=pci_bar64(&d,0);if(!m||m>0xffffffffull)return 0;mmio=(volatile uint8_t*)(uintptr_t)m;pci_enable(&d,0x0006);wr(REG_IMC,0xffffffffu);(void)rd(REG_ICR);uint32_t ral=rd(REG_RAL),rah=rd(REG_RAH);if(!(rah&0x80000000u)&&!(ral|rah))return 0;mac[0]=(uint8_t)ral;mac[1]=(uint8_t)(ral>>8);mac[2]=(uint8_t)(ral>>16);mac[3]=(uint8_t)(ral>>24);mac[4]=(uint8_t)rah;mac[5]=(uint8_t)(rah>>8);zero(rx,sizeof(rx));zero(txd,sizeof(txd));for(unsigned i=0;i<N;i++){rx[i].addr=(uint64_t)(uintptr_t)rxb[i];rx[i].status=0;txd[i].status=1;}uint64_t ra=(uint64_t)(uintptr_t)rx,ta=(uint64_t)(uintptr_t)txd;wr(REG_RDBAL,(uint32_t)ra);wr(REG_RDBAH,(uint32_t)(ra>>32));wr(REG_RDLEN,sizeof(rx));wr(REG_RDH,0);wr(REG_RDT,N-1);wr(REG_TDBAL,(uint32_t)ta);wr(REG_TDBAH,(uint32_t)(ta>>32));wr(REG_TDLEN,sizeof(txd));wr(REG_TDH,0);wr(REG_TDT,0);wr(REG_TIPG,0x0060200a);wr(REG_TCTL,TCTL_EN|TCTL_PSP|(15u<<4)|(64u<<12));wr(REG_RCTL,RCTL_EN|RCTL_BAM|RCTL_SECRC);rxi=0;txi=0;up=1;return 1;}
static int tx(const void *data,size_t len){if(!up||len<14||len>1514)return 0;unsigned s=txi;if(!(txd[s].status&1)){for(unsigned k=0;k<100000&&!(txd[s].status&1);k++)__asm__ volatile("pause");if(!(txd[s].status&1))return 0;}cp(txb[s],data,len);txd[s].addr=(uint64_t)(uintptr_t)txb[s];txd[s].len=(uint16_t)len;txd[s].cso=0;txd[s].cmd=0x0b;txd[s].status=0;txd[s].css=0;txd[s].special=0;txi=(s+1)%N;wr(REG_TDT,txi);return 1;}
static void poll(netdev_rx_fn cb){if(!up)return;for(unsigned guard=0;guard<N&& (rx[rxi].status&1);guard++){size_t len=rx[rxi].len;if(len>=14&&len<=BUF)cb(rxb[rxi],len);rx[rxi].status=0;unsigned done=rxi;rxi=(rxi+1)%N;wr(REG_RDT,done);}}
static const char *name(void){return model;}
/* The dispatcher needs a static name field; model-specific text is exported through this wrapper. */
const NetDriver e1000_driver={"Intel e1000/e1000e",name,init,tx,poll};
