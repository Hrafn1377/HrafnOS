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

// Free a process address space created by vmm_create_address_space: releases
// every user frame + page table under PML4[1..511] and the PML4 frame itself,
// leaving the shared kernel mapping (PML4[0]) untouched. The space must NOT be
// the active CR3 when this is called.
void vmm_destroy_address_space(uint64_t* root);

// Deep-copy a process address space: every mapped user page (PML4[1..511]) is
// copied into a fresh frame in a new space that shares the kernel mapping
// (PML4[0]). The mirror image of vmm_destroy_address_space.
uint64_t* vmm_clone_address_space(uint64_t* src);

uint64_t* vmm_kernel_space();          // the kernel's PML4
void      vmm_switch(uint64_t* root);  // load CR3
