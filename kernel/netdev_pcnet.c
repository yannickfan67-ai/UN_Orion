#include <stdint.h>
#include <stddef.h>
#include "io.h"
#include "pci.h"
#include "netdev_internal.h"
#define N 8
#define BUF 2048
typedef struct __attribute__((packed)){uint16_t mode;uint8_t rlen,tlen;uint16_t padr[3];uint16_t res;uint16_t ladrf[4];uint32_t rdra,tdra;} Init;
typedef struct __attribute__((packed)){uint32_t addr;int16_t bcnt;uint16_t status;uint32_t mcnt;uint32_t res;} Desc;
static uint16_t base;static Init ib __attribute__((aligned(16)));static Desc rx[N] __attribute__((aligned(16))),txd[N] __attribute__((aligned(16)));static uint8_t rxb[N][BUF] __attribute__((aligned(16))),txb[N][BUF] __attribute__((aligned(16)));static unsigned rxi,txi;static int up;
static void cp(void*d,const void*s,size_t n){uint8_t*a=d;const uint8_t*b=s;while(n--)*a++=*b++;}static void zero(void*d,size_t n){uint8_t*p=d;while(n--)*p++=0;}
static void rap(uint16_t r){outw((uint16_t)(base+0x12),r);}static uint16_t csr_r(uint16_t r){rap(r);return inw((uint16_t)(base+0x10));}static void csr_w(uint16_t r,uint16_t v){rap(r);outw((uint16_t)(base+0x10),v);}static void bcr_w(uint16_t r,uint16_t v){rap(r);outw((uint16_t)(base+0x16),v);}
static int init(uint8_t mac[6]){PciDevice d;if(!pci_find(0x1022,0x2000,&d))return 0;uint32_t bar=pci_bar32(&d,0);if(!(bar&1))return 0;base=(uint16_t)(bar&~3u);pci_enable(&d,0x0005);(void)inw((uint16_t)(base+0x14));for(int i=0;i<6;i++)mac[i]=inb((uint16_t)(base+i));bcr_w(20,2);zero(&ib,sizeof(ib));zero(rx,sizeof(rx));zero(txd,sizeof(txd));ib.mode=0;ib.rlen=(uint8_t)(3u<<4);ib.tlen=(uint8_t)(3u<<4);ib.padr[0]=(uint16_t)(mac[0]|((uint16_t)mac[1]<<8));ib.padr[1]=(uint16_t)(mac[2]|((uint16_t)mac[3]<<8));ib.padr[2]=(uint16_t)(mac[4]|((uint16_t)mac[5]<<8));ib.rdra=(uint32_t)(uintptr_t)rx;ib.tdra=(uint32_t)(uintptr_t)txd;for(unsigned i=0;i<N;i++){rx[i].addr=(uint32_t)(uintptr_t)rxb[i];rx[i].bcnt=(int16_t)(0xf000u|((uint16_t)(-BUF)&0x0fffu));rx[i].status=0x8000;rx[i].mcnt=0;txd[i].status=0;}uint32_t ia=(uint32_t)(uintptr_t)&ib;csr_w(1,(uint16_t)ia);csr_w(2,(uint16_t)(ia>>16));csr_w(0,0x0001);int ok=0;for(unsigned i=0;i<1000000;i++){if(csr_r(0)&0x0100){ok=1;break;}__asm__ volatile("pause");}if(!ok)return 0;csr_w(0,0x0002);for(unsigned i=0;i<100000;i++){uint16_t s=csr_r(0);if((s&0x0030)==0x0030)break;}rxi=txi=0;up=1;return 1;}
static int tx(const void *data,size_t len){if(!up||len<14||len>1514)return 0;unsigned s=txi;if(txd[s].status&0x8000)return 0;cp(txb[s],data,len);txd[s].addr=(uint32_t)(uintptr_t)txb[s];txd[s].bcnt=(int16_t)(0xf000u|((uint16_t)(-(int)len)&0x0fffu));txd[s].mcnt=0;txd[s].status=0x8300;txi=(s+1)%N;csr_w(0,0x0008);return 1;}
static void poll(netdev_rx_fn cb){if(!up)return;for(unsigned guard=0;guard<N;guard++){Desc*d=&rx[rxi];if(d->status&0x8000)break;uint16_t st=d->status;size_t len=d->mcnt&0x0fffu;if(!(st&0x4000)&&(st&0x0300)==0x0300&&len>=18&&len<=BUF)cb(rxb[rxi],len-4);d->mcnt=0;d->status=0x8000;rxi=(rxi+1)%N;}}
const NetDriver pcnet_driver={"AMD PCnet-PCI II / PCnet-FAST III",0,init,tx,poll};
