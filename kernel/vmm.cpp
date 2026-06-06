#include "vmm.hpp"
#include "pmm.hpp"

#define HUGE_PAGE 0x80         // PS bit: PD entry maps a 2 MiB page
#define ENTRIES   512
#define ADDR_MASK ~0xFFFULL    // strip flag bits to get the physical address

// This module assumes the page-table frames it allocates from the PMM are
// identity-mapped (physical address == accessible pointer). vmm_init keeps
// the first 1 GiB identity-mapped, and every table frame lives in low RAM,
// so a frame's physical address can be used directly as a pointer.

static uint64_t* pml4 = nullptr;

static inline uint64_t pml4_index(uint64_t v) { return (v >> 39) & 0x1FF; }
static inline uint64_t pdpt_index(uint64_t v) { return (v >> 30) & 0x1FF; }
static inline uint64_t pd_index(uint64_t v)   { return (v >> 21) & 0x1FF; }
static inline uint64_t pt_index(uint64_t v)   { return (v >> 12) & 0x1FF; }

static uint64_t* alloc_table() {
    uint64_t* t = (uint64_t*)pmm_alloc_frame();
    for (int i = 0; i < ENTRIES; i++) t[i] = 0;
    return t;
}

// Returns the next-level table, creating + linking it if not present.
static uint64_t* next_table(uint64_t* table, uint64_t index) {
    if (!(table[index] & PAGE_PRESENT)) {
        uint64_t* nt = alloc_table();
        table[index] = ((uint64_t)nt) | PAGE_PRESENT | PAGE_WRITABLE;
        return nt;
    }
    return (uint64_t*)(table[index] & ADDR_MASK);
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t* pdpt = next_table(pml4, pml4_index(virt));
    uint64_t* pd   = next_table(pdpt, pdpt_index(virt));
    uint64_t* pt   = next_table(pd,   pd_index(virt));
    pt[pt_index(virt)] = (phys & ADDR_MASK) | flags | PAGE_PRESENT;

    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");   // flush stale TLB
}

void vmm_init() {
    pml4 = alloc_table();

    // Identity-map the first 1 GiB with 2 MiB huge pages: one PDPT entry
    // pointing at one PD whose 512 entries each map a 2 MiB page.
    uint64_t* pdpt = alloc_table();
    uint64_t* pd   = alloc_table();
    pml4[0] = ((uint64_t)pdpt) | PAGE_PRESENT | PAGE_WRITABLE;
    pdpt[0] = ((uint64_t)pd)   | PAGE_PRESENT | PAGE_WRITABLE;
    for (uint64_t i = 0; i < ENTRIES; i++) {
        pd[i] = (i * 0x200000ULL) | PAGE_PRESENT | PAGE_WRITABLE | HUGE_PAGE;
    }

    // Activate. Safe because the new map mirrors the region the kernel,
    // stack, and tables currently occupy.
    asm volatile("mov %0, %%cr3" : : "r"((uint64_t)pml4) : "memory");
}