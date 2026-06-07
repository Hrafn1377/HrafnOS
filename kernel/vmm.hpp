#pragma once
#include <stdint.h>

#define PAGE_PRESENT  0x1
#define PAGE_WRITABLE 0x2
#define PAGE_USER     0x4

void vmm_init();

// Map into the kernel's own address space (used for the heap).
void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);

// Map into a specific address space (process). Page-table frames are reached
// via their identity mapping, so this works regardless of the active CR3.
void vmm_map_page_in(uint64_t* root, uint64_t virt, uint64_t phys, uint64_t flags);

// Create a fresh address space that shares the kernel (PML4[0]) but has its
// own private user region.
uint64_t* vmm_create_address_space();

uint64_t* vmm_kernel_space();          // the kernel's PML4
void      vmm_switch(uint64_t* root);  // load CR3
