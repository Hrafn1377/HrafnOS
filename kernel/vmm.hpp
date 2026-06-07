#pragma once
#include <stdint.h>

#define PAGE_PRESENT  0x1
#define PAGE_WRITABLE 0x2
#define PAGE_USER     0x4

void vmm_init();
void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
