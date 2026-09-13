#ifndef ORION_PMM_H
#define ORION_PMM_H
#include <stdint.h>
#include "bootinfo.h"
void pmm_init(const OrionBootInfo *bi);
uint64_t pmm_alloc_page(void);
int pmm_free_page(uint64_t page);
uint64_t pmm_total_pages(void);
uint64_t pmm_used_pages(void);
uint64_t pmm_free_pages(void);
#endif
