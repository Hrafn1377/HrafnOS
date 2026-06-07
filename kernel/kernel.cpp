#include "serial.hpp"
#include "gdt.hpp"
#include "idt.hpp"
#include "pic.hpp"
#include "pmm.hpp"
#include "vmm.hpp"
#include "heap.hpp"
#include "sched.hpp"

extern "C" uint8_t user_a_start[];
extern "C" uint8_t user_a_end[];
extern "C" uint8_t user_b_start[];
extern "C" uint8_t user_b_end[];

#define USER_CODE      0x8000000000ULL     // 512 GiB -> PML4[1] (private per process)
#define USER_STACK_TOP 0x8000005000ULL

static void kmemcpy(void* dst, const void* src, uint64_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (uint64_t i = 0; i < n; i++) d[i] = s[i];
}

// Build a process: its own address space, with `prog` copied to a code page and
// stack pages, all at the SAME virtual addresses as every other process.
static uint64_t* make_process(uint8_t* prog_start, uint8_t* prog_end) {
    uint64_t* space = vmm_create_address_space();

    // Code page: populate the physical frame via its identity address, then
    // map it into this process at USER_CODE.
    uint64_t code = (uint64_t)pmm_alloc_frame();
    kmemcpy((void*)code, prog_start, (uint64_t)(prog_end - prog_start));
    vmm_map_page_in(space, USER_CODE, code, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    // Stack pages.
    for (uint64_t off = 0; off < 0x4000; off += FRAME_SIZE) {
        uint64_t f = (uint64_t)pmm_alloc_frame();
        vmm_map_page_in(space, (USER_STACK_TOP - 0x4000) + off, f,
                        PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }
    return space;
}

extern "C" void kmain(uint64_t mb_info) {
    serial_init();
    kprint("HrafnOS booting...\n");

    gdt_init();
    idt_init();
    pic_remap();
    pit_init(100);

    pmm_init(mb_info);
    uint64_t total_mib = (pmm_total_frame_count() * FRAME_SIZE) / (1024 * 1024);
    kprint("Physical memory: ");
    kprint_uint((uint32_t)total_mib);
    kprint(" MiB, ");
    kprint_uint((uint32_t)pmm_free_frame_count());
    kprint(" frames free\n");

    vmm_init();
    heap_init();
    kprint("Heap ready.\n");

    // Two processes, each in its own address space, both with code at the
    // identical virtual address 0x8000000000 but different physical memory.
    uint64_t* spaceA = make_process(user_a_start, user_a_end);
    uint64_t* spaceB = make_process(user_b_start, user_b_end);

    sched_init();
    task_create_user(spaceA, USER_CODE, USER_STACK_TOP);
    task_create_user(spaceB, USER_CODE, USER_STACK_TOP);

    kprint("Two processes, same vaddr (0x8000000000), separate address spaces:\n");

    asm volatile("sti");
    while (true) asm volatile("hlt");
}
