#include <stdint.h>
#include "bootinfo.h"
#include "pmm.h"
#define PAGE_SIZE 4096ULL
#define PMM_MIN_ADDR 0x01000000ULL

typedef struct{uint32_t type;uint32_t pad;uint64_t physical_start;uint64_t virtual_start;uint64_t pages;uint64_t attr;} EfiMemDesc;
static const OrionBootInfo *g_bi;
static uint64_t total_pages_count, used_pages_count;
static uint64_t scan_off, scan_addr, scan_end;
extern char __kernel_start[], __kernel_end[];
static uint64_t align_up(uint64_t v,uint64_t a){return (v+a-1)&~(a-1);}
static int overlap(uint64_t a0,uint64_t a1,uint64_t b0,uint64_t b1){return a0<b1&&b0<a1;}
static int reserved(uint64_t p){
    uint64_t e=p+PAGE_SIZE;
    if(overlap(p,e,(uint64_t)(uintptr_t)__kernel_start,(uint64_t)(uintptr_t)__kernel_end))return 1;
    if(g_bi){
        uint64_t bi=(uint64_t)(uintptr_t)g_bi;
        if(overlap(p,e,bi,bi+sizeof(*g_bi)))return 1;
        if(overlap(p,e,g_bi->memory_map,g_bi->memory_map+g_bi->memory_map_size))return 1;
        if(overlap(p,e,g_bi->framebuffer_base,g_bi->framebuffer_base+g_bi->framebuffer_size))return 1;
    }
    return 0;
}
void pmm_init(const OrionBootInfo *bi){
    g_bi=bi; total_pages_count=used_pages_count=scan_off=scan_addr=scan_end=0;
    if(!bi||!bi->memory_descriptor_size)return;
    for(uint64_t off=0;off+sizeof(EfiMemDesc)<=bi->memory_map_size;off+=bi->memory_descriptor_size){
        const EfiMemDesc*d=(const EfiMemDesc*)(uintptr_t)(bi->memory_map+off);
        if(d->type!=7||d->pages==0)continue;
        uint64_t start=d->physical_start,end=start+d->pages*PAGE_SIZE;
        if(end<=PMM_MIN_ADDR)continue; if(start<PMM_MIN_ADDR)start=align_up(PMM_MIN_ADDR,PAGE_SIZE);
        if(end>start)total_pages_count+=(end-start)/PAGE_SIZE;
    }
}
uint64_t pmm_alloc_page(void){
    if(!g_bi||!g_bi->memory_descriptor_size)return 0;
    for(;;){
        while(scan_addr&&scan_addr+PAGE_SIZE<=scan_end){uint64_t p=scan_addr;scan_addr+=PAGE_SIZE;if(reserved(p))continue;used_pages_count++;return p;}
        scan_addr=scan_end=0;
        if(scan_off+sizeof(EfiMemDesc)>g_bi->memory_map_size)return 0;
        const EfiMemDesc*d=(const EfiMemDesc*)(uintptr_t)(g_bi->memory_map+scan_off); scan_off+=g_bi->memory_descriptor_size;
        if(d->type!=7||d->pages==0)continue;
        uint64_t start=d->physical_start,end=start+d->pages*PAGE_SIZE;
        if(end<=PMM_MIN_ADDR)continue; if(start<PMM_MIN_ADDR)start=align_up(PMM_MIN_ADDR,PAGE_SIZE);
        if(start>=end)continue; scan_addr=start;scan_end=end;
    }
}
uint64_t pmm_total_pages(void){return total_pages_count;}
uint64_t pmm_used_pages(void){return used_pages_count;}
uint64_t pmm_free_pages(void){return total_pages_count>used_pages_count?total_pages_count-used_pages_count:0;}
