#include "vmm.hpp"
#include "pmm.hpp"

#define HUGE_PAGE 0x80
#define ENTRIES   512
#define ADDR_MASK ~0xFFFULL

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

// Returns the next-level table, creating + linking it if absent. The USER bit
// is propagated into intermediate entries so a user mapping is reachable at
// CPL 3 (the page walk ANDs US across all levels). Kernel pages under a shared
// intermediate stay protected via their own US=0 entries.
static uint64_t* next_table(uint64_t* table, uint64_t index, uint64_t flags) {
    uint64_t want = PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    if (!(table[index] & PAGE_PRESENT)) {
        uint64_t* nt = alloc_table();
        table[index] = ((uint64_t)nt) | want;
        return nt;
    }
    table[index] |= (flags & PAGE_USER);
    return (uint64_t*)(table[index] & ADDR_MASK);
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t* pdpt = next_table(pml4, pml4_index(virt), flags);
    uint64_t* pd   = next_table(pdpt, pdpt_index(virt), flags);
    uint64_t* pt   = next_table(pd,   pd_index(virt),   flags);
    pt[pt_index(virt)] = (phys & ADDR_MASK) | flags | PAGE_PRESENT;
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_init() {
    pml4 = alloc_table();
    uint64_t* pdpt = alloc_table();
    uint64_t* pd   = alloc_table();
    pml4[0] = ((uint64_t)pdpt) | PAGE_PRESENT | PAGE_WRITABLE;
    pdpt[0] = ((uint64_t)pd)   | PAGE_PRESENT | PAGE_WRITABLE;
    for (uint64_t i = 0; i < ENTRIES; i++)
        pd[i] = (i * 0x200000ULL) | PAGE_PRESENT | PAGE_WRITABLE | HUGE_PAGE;
    asm volatile("mov %0, %%cr3" : : "r"((uint64_t)pml4) : "memory");
}
