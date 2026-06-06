#include "serial.hpp"
#include "idt.hpp"
#include "pic.hpp"
#include "pmm.hpp"
#include "vmm.hpp"
#include "heap.hpp"
#include "sched.hpp"
#include "spinlock.hpp"

static spinlock print_lock = { 0 };

static void say(const char* s) {
    spin_lock(&print_lock);
    kprint(s);
    spin_unlock(&print_lock);
}

// Prints [A] twice a second, forever.
static void task_a() {
    for (;;) {
        say("[A]");
        task_sleep(50);          // 50 ticks @ 100 Hz = 0.5 s
    }
}

// Prints [B] once a second, forever.
static void task_b() {
    for (;;) {
        say("[B]");
        task_sleep(100);         // 1.0 s
    }
}

// Prints [C] three times, then exits — after which no more [C] appears.
static void task_c() {
    for (int i = 0; i < 3; i++) {
        say("[C]");
        task_sleep(75);          // 0.75 s
    }
    say("[C done]");
    task_exit();                 // never returns
}

extern "C" void kmain(uint64_t mb_info) {
    serial_init();
    kprint("HrafnOS booting...\n");

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
    task_create(task_a);
    task_create(task_b);
    task_create(task_c);
    kprint("Scheduler: 3 tasks (sleep/exit/locking). Go.\n");

    asm volatile("sti");
    while (true) asm volatile("hlt");
}