#include "serial.hpp"
#include "gdt.hpp"
#include "idt.hpp"
#include "pic.hpp"
#include "pmm.hpp"
#include "vmm.hpp"
#include "heap.hpp"
#include "sched.hpp"
#include "spinlock.hpp"
#include "syscall.hpp"

static spinlock print_lock = { 0 };

// Prints directly, in the kernel (ring 0).
static void task_direct() {
    for (;;) {
        spin_lock(&print_lock);
        kprint("[direct]");
        spin_unlock(&print_lock);
        task_sleep(60);
    }
}

// Prints by asking the kernel through a system call (int $0x80) instead of
// calling kprint itself — exercising the full syscall path.
static void task_syscall() {
    for (;;) {
        spin_lock(&print_lock);
        sys_write("[syscall]");
        spin_unlock(&print_lock);
        task_sleep(60);
    }
}

extern "C" void kmain(uint64_t mb_info) {
    serial_init();
    kprint("HrafnOS booting...\n");

    gdt_init();
    kprint("GDT/TSS installed (kernel + user segments).\n");

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
    kprint("Paging: kernel page tables active.\n");

    heap_init();
    kprint("Heap ready.\n");

    sched_init();
    task_create(task_direct);
    task_create(task_syscall);
    kprint("Scheduler: direct vs. syscall printing. Go.\n");

    asm volatile("sti");
    while (true) asm volatile("hlt");
}
