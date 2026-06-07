#include "gdt.hpp"

// Long-mode Task State Segment. Only rsp0 / the IST entries matter in 64-bit
// mode; rsp0 is the stack the CPU loads when entering ring 0 from ring 3.
struct tss_t {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

// Seven 8-byte slots: null, kcode, kdata, ucode, udata, then the TSS
// descriptor which is 16 bytes (two slots).
static uint64_t gdt[7] __attribute__((aligned(16)));
static tss_t    tss __attribute__((aligned(16)));

// Dedicated kernel stack the TSS hands the CPU on a privilege transition.
static uint8_t  kstack0[16384] __attribute__((aligned(16)));

struct gdt_ptr_t {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

void gdt_init() {
    gdt[0] = 0x0000000000000000;          // null
    gdt[1] = 0x00af9a000000ffff;          // 0x08 kernel code (DPL0)
    gdt[2] = 0x00af92000000ffff;          // 0x10 kernel data (DPL0)
    gdt[3] = 0x00affa000000ffff;          // 0x18 user code   (DPL3)
    gdt[4] = 0x00aff2000000ffff;          // 0x20 user data   (DPL3)

    // TSS setup.
    tss.rsp0        = (uint64_t)kstack0 + sizeof(kstack0);
    tss.iopb_offset = sizeof(tss_t);

    // 16-byte TSS system descriptor at gdt[5]/gdt[6], selector 0x28.
    uint64_t base  = (uint64_t)&tss;
    uint32_t limit = sizeof(tss_t) - 1;

    uint64_t low = 0;
    low |= (uint64_t)(limit & 0xFFFF);
    low |= (base & 0xFFFFFF) << 16;
    low |= (uint64_t)0x89 << 40;          // present, type 9 (available 64-bit TSS)
    low |= (uint64_t)((limit >> 16) & 0xF) << 48;
    low |= ((base >> 24) & 0xFF) << 56;
    gdt[5] = low;
    gdt[6] = (base >> 32) & 0xFFFFFFFF;

    gdt_ptr_t gp;
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint64_t)gdt;
    asm volatile("lgdt %0" : : "m"(gp));

    // Reload data segments to the kernel data selector. CS is unchanged
    // (0x08 is byte-identical to the boot GDT's entry), so no far jump needed.
    asm volatile(
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        ::: "ax");

    // Load the task register with the TSS selector.
    asm volatile("ltr %%ax" : : "a"((uint16_t)0x28));
}
