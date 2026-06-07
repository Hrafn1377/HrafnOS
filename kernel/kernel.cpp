#include "serial.hpp"
#include "gdt.hpp"
#include "idt.hpp"
#include "pic.hpp"
#include "pmm.hpp"
#include "vmm.hpp"
#include "heap.hpp"
#include "sched.hpp"

extern "C" uint8_t user_program_start[];
extern "C" uint8_t user_program_end[];

#define USER_CODE      0x800000000ULL
#define USER_STACK_TOP 0x800005000ULL

static void kmemcpy(void* dst, const void* src, uint64_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (uint64_t i = 0; i < n; i++) d[i] = s[i];
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

    // Map and load the userspace program.
    void* code_frame = pmm_alloc_frame();
    vmm_map_page(USER_CODE, (uint64_t)code_frame,
                 PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    for (uint64_t off = 0; off < 0x4000; off += FRAME_SIZE) {
        void* f = pmm_alloc_frame();
        vmm_map_page((USER_STACK_TOP - 0x4000) + off, (uint64_t)f,
                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }
    kmemcpy((void*)USER_CODE, user_program_start,
            (uint64_t)(user_program_end - user_program_start));

    sched_init();
    task_create_user(USER_CODE, USER_STACK_TOP);

    kprint("\nRing-3 shell echo. Type letters (echoed UPPERCASE); 'q' quits.\n");
    kprint("> ");

    asm volatile("sti");
    while (true) asm volatile("hlt");
}
