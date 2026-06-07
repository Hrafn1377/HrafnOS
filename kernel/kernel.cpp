#include "serial.hpp"
#include "gdt.hpp"
#include "idt.hpp"
#include "pic.hpp"
#include "pmm.hpp"
#include "vmm.hpp"
#include "heap.hpp"
#include "sched.hpp"
#include "spinlock.hpp"

// The ring-3 program, supplied as raw bytes by user.asm.
extern "C" uint8_t user_program_start[];
extern "C" uint8_t user_program_end[];

#define USER_CODE      0x800000000ULL          // 32 GiB: clear of identity + heap
#define USER_STACK_TOP 0x800005000ULL          // 4 stack pages below this

static spinlock print_lock = { 0 };

static void kmemcpy(void* dst, const void* src, uint64_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (uint64_t i = 0; i < n; i++) d[i] = s[i];
}

// A ring-0 kernel task, for contrast with the ring-3 one.
static void task_kernel() {
    for (;;) {
        spin_lock(&print_lock);
        kprint("[kernel]");
        spin_unlock(&print_lock);
        task_sleep(80);
    }
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

    // --- set up userspace ---
    // One user-accessible code page.
    void* code_frame = pmm_alloc_frame();
    vmm_map_page(USER_CODE, (uint64_t)code_frame,
                 PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    // Four user-accessible stack pages just below USER_STACK_TOP.
    for (uint64_t off = 0; off < 0x4000; off += FRAME_SIZE) {
        void* f = pmm_alloc_frame();
        vmm_map_page((USER_STACK_TOP - 0x4000) + off, (uint64_t)f,
                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    // Copy the program into the user code page.
    kmemcpy((void*)USER_CODE, user_program_start,
            (uint64_t)(user_program_end - user_program_start));
    kprint("Userspace program mapped at 0x800000000.\n");

    sched_init();
    task_create(task_kernel);                       // ring 0
    task_create_user(USER_CODE, USER_STACK_TOP);    // ring 3
    kprint("Scheduler: kernel task + ring-3 user task. Go.\n");

    asm volatile("sti");
    while (true) asm volatile("hlt");
}
