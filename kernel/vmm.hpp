#pragma once
#include <stdint.h>

#define PAGE_PRESENT  0x1
#define PAGE_WRITABLE 0x2

// Builds kernel-owned page tables (identity-mapping the first 1 GiB) and
// loads them into CR3, replacing the boot trampoline's tables.
void vmm_init();

// Maps a 4 KiB virtual page to a physical frame, creating intermediate
// page tables from the PMM as needed.
//
// CONSTRAINT: do not call this for virtual addresses in the first 1 GiB —
// that range is mapped with 2 MiB huge pages, and walking into a huge-page
// entry as if it were a page table would corrupt memory. Map new pages at
// virtual addresses >= 1 GiB.
void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);