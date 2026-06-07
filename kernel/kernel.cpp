#include "serial.hpp"
#include "gdt.hpp"
#include "idt.hpp"
#include "pic.hpp"
#include "pmm.hpp"
#include "vmm.hpp"
#include "heap.hpp"
#include "sched.hpp"
#include "elf.hpp"
#include "ramdisk.hpp"

#define USER_STACK_TOP 0x8000100000ULL     // above the loaded ELF segments

// Look a program up in the ramdisk, load it into a fresh address space, give it
// a user stack, and queue it as a ring-3 task. (This is the path exec will reuse.)
static void spawn(const char* name) {
    uint8_t* elf = ramdisk_lookup(name);
    if (!elf) {
        kprint("spawn: not found: ");
        kprint(name);
        kprint_char('\n');
        return;
    }

    uint64_t  entry = 0;
    uint64_t* space = elf_load(elf, &entry);
    if (!space) {
        kprint("spawn: load failed: ");
        kprint(name);
        kprint_char('\n');
        return;
    }

    for (uint64_t off = 0; off < 0x4000; off += FRAME_SIZE) {
        uint64_t f = (uint64_t)pmm_alloc_frame();
        vmm_map_page_in(space, (USER_STACK_TOP - 0x4000) + off, f,
                        PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    task_create_user(space, entry, USER_STACK_TOP);
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

    // Show the ramdisk directory, straight off the table.
    kprint("ramdisk: ");
    for (uint32_t i = 0; i < ramdisk_count(); i++) {
        const ramdisk_entry* e = ramdisk_get(i);
        kprint(e->name);
        kprint(" (");
        kprint_uint((uint32_t)(e->end - e->start));
        kprint(" bytes)");
        if (i + 1 < ramdisk_count()) kprint(", ");
    }
    kprint_char('\n');

    sched_init();
    spawn("one");
    spawn("two");
    kprint("Running ramdisk programs at ring 3:\n");

    asm volatile("sti");
    while (true) asm volatile("hlt");
}
