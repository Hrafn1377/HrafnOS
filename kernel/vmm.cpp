#include "vmm.hpp"
#include "pmm.hpp"

#define HUGE_PAGE 0x80
#define ENTRIES   512
#define ADDR_MASK ~0xFFFULL

static uint64_t* kernel_pml4 = nullptr;

static inline uint64_t pml4_index(uint64_t v) { return (v >> 39) & 0x1FF; }
static inline uint64_t pdpt_index(uint64_t v) { return (v >> 30) & 0x1FF; }
static inline uint64_t pd_index(uint64_t v)   { return (v >> 21) & 0x1FF; }
static inline uint64_t pt_index(uint64_t v)   { return (v >> 12) & 0x1FF; }

static uint64_t* alloc_table() {
    uint64_t* t = (uint64_t*)pmm_alloc_frame();   // identity-accessible (< 1 GiB)
    for (int i = 0; i < ENTRIES; i++) t[i] = 0;
    return t;
}

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

static void map_in(uint64_t* root, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t* pdpt = next_table(root, pml4_index(virt), flags);
    uint64_t* pd   = next_table(pdpt, pdpt_index(virt), flags);
    uint64_t* pt   = next_table(pd,   pd_index(virt),   flags);
    pt[pt_index(virt)] = (phys & ADDR_MASK) | flags | PAGE_PRESENT;
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    map_in(kernel_pml4, virt, phys, flags);
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_map_page_in(uint64_t* root, uint64_t virt, uint64_t phys, uint64_t flags) {
    map_in(root, virt, phys, flags);   // TLB flushes when this space is switched to
}

uint64_t* vmm_create_address_space() {
    uint64_t* root = alloc_table();
    root[0] = kernel_pml4[0];          // share the kernel's PML4[0] subtree
    return root;
}

uint64_t* vmm_kernel_space() { return kernel_pml4; }

void vmm_switch(uint64_t* root) {
    asm volatile("mov %0, %%cr3" : : "r"((uint64_t)root) : "memory");
}

void vmm_init() {
    kernel_pml4 = alloc_table();
    uint64_t* pdpt = alloc_table();
    uint64_t* pd   = alloc_table();
    kernel_pml4[0] = ((uint64_t)pdpt) | PAGE_PRESENT | PAGE_WRITABLE;  // US=0: kernel
    pdpt[0]        = ((uint64_t)pd)   | PAGE_PRESENT | PAGE_WRITABLE;
    for (uint64_t i = 0; i < ENTRIES; i++)
        pd[i] = (i * 0x200000ULL) | PAGE_PRESENT | PAGE_WRITABLE | HUGE_PAGE;
    asm volatile("mov %0, %%cr3" : : "r"((uint64_t)kernel_pml4) : "memory");
}
