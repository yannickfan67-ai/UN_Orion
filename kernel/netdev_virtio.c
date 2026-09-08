#include <stdint.h>
#include <stddef.h>
#include "io.h"
#include "pci.h"
#include "netdev_internal.h"
#define QMAX 256
#define PAGE 4096
#define RINGSZ 16384
#define BUF 2048
#define VIRTIO_F_MAC (1u<<5)
typedef struct __attribute__((packed)){uint64_t addr;uint32_t len;uint16_t flags,next;} VDesc;
typedef struct __attribute__((packed)){uint32_t id,len;} VUsed;
static uint16_t base;static int up;static uint16_t qn_rx,qn_tx;static uint8_t ring_rx[RINGSZ] __attribute__((aligned(PAGE))),ring_tx[RINGSZ] __attribute__((aligned(PAGE)));static uint8_t rxb[QMAX][BUF+10] __attribute__((aligned(16)));static uint8_t txb[BUF+10] __attribute__((aligned(16)));static uint16_t tx_used_seen;
static void cp(void*d,const void*s,size_t n){uint8_t*a=d;const uint8_t*b=s;while(n--)*a++=*b++;}static void zero(void*d,size_t n){uint8_t*p=d;while(n--)*p++=0;}
static VDesc *desc(uint8_t*r){return (VDesc*)r;}static volatile uint16_t *avail_flags(uint8_t*r,uint16_t n){(void)n;return (volatile uint16_t*)(r+16u*n);}static volatile uint16_t *avail_idx(uint8_t*r,uint16_t n){return avail_flags(r,n)+1;}static volatile uint16_t *avail_ring(uint8_t*r,uint16_t n){return avail_flags(r,n)+2;}static uintptr_t used_off(uint16_t n){uintptr_t x=16u*n+4u+2u*n+2u;return (x+PAGE-1)&~(PAGE-1);}static volatile uint16_t *used_idx(uint8_t*r,uint16_t n){return (volatile uint16_t*)(r+used_off(n)+2);}static volatile VUsed *used_ring(uint8_t*r,uint16_t n){return (volatile VUsed*)(r+used_off(n)+4);}
static int setup_q(uint16_t q,uint8_t*r,uint16_t *nout){outw((uint16_t)(base+0x0e),q);uint16_t n=inw((uint16_t)(base+0x0c));if(!n||n>QMAX)return 0;zero(r,RINGSZ);outl((uint16_t)(base+0x08),(uint32_t)((uintptr_t)r>>12));*nout=n;return 1;}
static int init(uint8_t mac[6]){PciDevice d;if(!pci_find(0x1af4,0x1000,&d))return 0;uint32_t bar=pci_bar32(&d,0);if(!(bar&1))return 0;base=(uint16_t)(bar&~3u);pci_enable(&d,0x0005);outb((uint16_t)(base+0x12),0);outb((uint16_t)(base+0x12),1);outb((uint16_t)(base+0x12),3);uint32_t f=inl(base);if(!(f&VIRTIO_F_MAC))return 0;outl((uint16_t)(base+0x04),VIRTIO_F_MAC);for(int i=0;i<6;i++)mac[i]=inb((uint16_t)(base+0x14+i));if(!setup_q(0,ring_rx,&qn_rx)||!setup_q(1,ring_tx,&qn_tx))return 0;VDesc *dr=desc(ring_rx);volatile uint16_t *ar=avail_ring(ring_rx,qn_rx);for(uint16_t i=0;i<qn_rx;i++){dr[i].addr=(uint64_t)(uintptr_t)rxb[i];dr[i].len=BUF+10;dr[i].flags=2;dr[i].next=0;ar[i]=i;}*avail_idx(ring_rx,qn_rx)=qn_rx;*avail_flags(ring_rx,qn_rx)=0;VDesc *dt=desc(ring_tx);dt[0].addr=(uint64_t)(uintptr_t)txb;dt[0].len=0;dt[0].flags=0;dt[0].next=0;*avail_idx(ring_tx,qn_tx)=0;tx_used_seen=*used_idx(ring_tx,qn_tx);outb((uint16_t)(base+0x12),7);outw((uint16_t)(base+0x10),0);up=1;return 1;}
static int tx(const void *data,size_t len){if(!up||len<14||len>BUF)return 0;volatile uint16_t *ui=used_idx(ring_tx,qn_tx);for(unsigned k=0;k<200000&&*ui!=tx_used_seen;k++){} /* drain prior completion */
    zero(txb,10);cp(txb+10,data,len);VDesc*d=desc(ring_tx);d[0].addr=(uint64_t)(uintptr_t)txb;d[0].len=(uint32_t)(len+10);d[0].flags=0;volatile uint16_t *ai=avail_idx(ring_tx,qn_tx),*ar=avail_ring(ring_tx,qn_tx);uint16_t a=*ai;ar[a%qn_tx]=0;__asm__ volatile("mfence":::"memory");*ai=(uint16_t)(a+1);outw((uint16_t)(base+0x10),1);uint16_t target=(uint16_t)(tx_used_seen+1);for(unsigned k=0;k<1000000;k++){if(*ui==target){tx_used_seen=target;return 1;}__asm__ volatile("pause");}return 0;}
static void poll(netdev_rx_fn cb){if(!up)return;static uint16_t seen;volatile uint16_t *ui=used_idx(ring_rx,qn_rx);volatile VUsed *ur=used_ring(ring_rx,qn_rx);volatile uint16_t *ai=avail_idx(ring_rx,qn_rx),*ar=avail_ring(ring_rx,qn_rx);while(seen!=*ui){VUsed e=ur[seen%qn_rx];uint16_t id=(uint16_t)e.id;if(id<qn_rx&&e.len>=24&&e.len<=BUF+10)cb(rxb[id]+10,e.len-10);uint16_t a=*ai;ar[a%qn_rx]=id;__asm__ volatile("mfence":::"memory");*ai=(uint16_t)(a+1);seen++;}if(seen)outw((uint16_t)(base+0x10),0);}
const NetDriver virtio_net_driver={"Paravirtualized Network (virtio-net legacy)",0,init,tx,poll};
