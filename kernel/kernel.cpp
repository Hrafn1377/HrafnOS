#include "serial.hpp"
#include "gdt.hpp"
#include "idt.hpp"
#include "pic.hpp"
#include "pmm.hpp"
#include "vmm.hpp"
#include "heap.hpp"
#include "sched.hpp"
#include "elf.hpp"

// The compiled user program, embedded by embed.asm via incbin.
extern "C" uint8_t user_elf_start[];
extern "C" uint8_t user_elf_end[];

#define USER_STACK_TOP 0x8000100000ULL     // above the loaded ELF segments

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

    // Parse and load the embedded ELF into its own address space.
    uint64_t  entry = 0;
    uint64_t* space = elf_load(user_elf_start, &entry);
    if (!space) {
        kprint("ELF load failed; halting.\n");
        for (;;) asm volatile("hlt");
    }

    // Give the process a user stack.
    for (uint64_t off = 0; off < 0x4000; off += FRAME_SIZE) {
        uint64_t f = (uint64_t)pmm_alloc_frame();
        vmm_map_page_in(space, (USER_STACK_TOP - 0x4000) + off, f,
                        PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    kprint("Loaded ELF (");
    kprint_uint((uint32_t)(user_elf_end - user_elf_start));
    kprint(" bytes), entry = ");
    kprint_ptr((void*)entry);
    kprint_char('\n');

    sched_init();
    task_create_user(space, entry, USER_STACK_TOP);
    kprint("Running compiled program at ring 3:\n");

    asm volatile("sti");
    while (true) asm volatile("hlt");
}
