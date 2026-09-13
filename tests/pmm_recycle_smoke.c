#include <stdint.h>
#include <stdio.h>
#include "bootinfo.h"
#include "pmm.h"

char __kernel_start[1];
char __kernel_end[1];

typedef struct {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t pages;
    uint64_t attr;
} TestMemDesc;

static int check(int ok, const char *what) {
    if (!ok) {
        fprintf(stderr, "PMM smoke failed: %s\n", what);
        return 0;
    }
    return 1;
}

int main(void) {
    TestMemDesc map[2] = {0};
    map[0].type = 7;
    map[0].physical_start = 0x01000000ULL;
    map[0].pages = 8;
    map[1].type = 11;
    map[1].physical_start = 0x02000000ULL;
    map[1].pages = 8;

    OrionBootInfo bi = {0};
    bi.memory_map = (uint64_t)(uintptr_t)map;
    bi.memory_map_size = sizeof(map);
    bi.memory_descriptor_size = sizeof(TestMemDesc);

    pmm_init(&bi);
    if (!check(pmm_total_pages() == 8, "conventional page count")) return 1;
    if (!check(pmm_used_pages() == 0, "initial used count")) return 1;

    uint64_t a = pmm_alloc_page();
    uint64_t b = pmm_alloc_page();
    if (!check(a == 0x01000000ULL, "first allocation")) return 1;
    if (!check(b == 0x01001000ULL, "second allocation")) return 1;
    if (!check(pmm_used_pages() == 2, "used count after allocation")) return 1;

    if (!check(pmm_free_page(a) == 1, "free allocated page")) return 1;
    if (!check(pmm_used_pages() == 1, "used count after free")) return 1;
    if (!check(pmm_free_page(a) == 0, "reject duplicate free")) return 1;
    if (!check(pmm_free_page(a + 1) == 0, "reject unaligned free")) return 1;

    uint64_t c = pmm_alloc_page();
    if (!check(c == a, "recycle freed page before scanning new pages")) return 1;
    if (!check(pmm_used_pages() == 2, "used count after recycle")) return 1;

    puts("PMM recycle smoke test passed");
    return 0;
}
