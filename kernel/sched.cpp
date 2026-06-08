#include "sched.hpp"
#include "heap.hpp"
#include "gdt.hpp"
#include "vmm.hpp"
#include "pmm.hpp"
#include "elf.hpp"
#include "ramdisk.hpp"
#include "userspace.hpp"
#include "serial.hpp"

#define STACK_SIZE 16384

static task*             current = nullptr;
static volatile uint64_t g_ticks = 0;

void sched_init() {
    task* boot = (task*)kmalloc(sizeof(task));
    boot->next       = boot;
    boot->state      = RUNNABLE;
    boot->wake_tick  = 0;
    boot->kstack_top = 0;
    boot->pml4       = vmm_kernel_space();   // idle runs in the kernel space
    current = boot;
}

task* task_create(void (*entry)()) {
    task*    t         = (task*)kmalloc(sizeof(task));
    uint64_t stack     = (uint64_t)kmalloc(STACK_SIZE);
    uint64_t stack_top = (stack + STACK_SIZE) & ~15ULL;

    uint64_t* sp = (uint64_t*)stack_top;
    *--sp = 0x10;                 // ss  (kernel data)
    *--sp = stack_top - 8;        // rsp
    *--sp = 0x202;                // rflags
    *--sp = 0x08;                 // cs  (kernel code)
    *--sp = (uint64_t)entry;      // rip
    *--sp = 0;                    // err_code
    *--sp = 0;                    // int_no
    for (int i = 0; i < 15; i++) *--sp = 0;

    t->rsp        = (uint64_t)sp;
    t->state      = RUNNABLE;
    t->wake_tick  = 0;
    t->kstack_top = stack_top;
    t->pml4       = vmm_kernel_space();
    t->next       = current->next;
    current->next = t;
    return t;
}

task* task_create_user(uint64_t* pml4, uint64_t entry, uint64_t user_stack_top) {
    task*    t          = (task*)kmalloc(sizeof(task));
    uint64_t kstack     = (uint64_t)kmalloc(STACK_SIZE);
    uint64_t kstack_top = (kstack + STACK_SIZE) & ~15ULL;

    uint64_t* sp = (uint64_t*)kstack_top;
    *--sp = 0x23;                 // ss  = user data | RPL 3
    *--sp = user_stack_top;       // rsp = user stack
    *--sp = 0x202;                // rflags (IF=1)
    *--sp = 0x1B;                 // cs  = user code | RPL 3
    *--sp = entry;                // rip
    *--sp = 0;                    // err_code
    *--sp = 0;                    // int_no
    for (int i = 0; i < 15; i++) *--sp = 0;

    t->rsp        = (uint64_t)sp;
    t->state      = RUNNABLE;
    t->wake_tick  = 0;
    t->kstack_top = kstack_top;
    t->pml4       = pml4;         // this process's private address space
    t->next       = current->next;
    current->next = t;
    return t;
}

uint64_t exec_current(const char* name) {
    // Copy the name out of user memory now, while the caller's address space is
    // still active. After the CR3 switch below, this same virtual address would
    // read the *new* program's memory.
    char nbuf[32];
    int  ni = 0;
    while (name[ni] && ni < 31) { nbuf[ni] = name[ni]; ni++; }
    nbuf[ni] = 0;

    uint8_t* elf = ramdisk_lookup(nbuf);
    if (!elf) return 0;

    // Load into a fresh address space. (elf bytes live in the kernel ramdisk,
    // and `name` is read here while the caller's address space is still active.)
    uint64_t  entry = 0;
    uint64_t* space = elf_load(elf, &entry);
    if (!space) return 0;

    // Give the new image a user stack.
    for (uint64_t off = 0; off < USER_STACK_SIZE; off += FRAME_SIZE) {
        uint64_t f = (uint64_t)pmm_alloc_frame();
        vmm_map_page_in(space, (USER_STACK_TOP - USER_STACK_SIZE) + off, f,
                        PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    // Replace this task's address space, remembering the old one to reclaim.
    uint64_t* old_space = current->pml4;
    current->pml4 = space;

    // Fabricate a fresh ring-3 entry frame at the top of our kernel stack,
    // exactly where the original syscall frame sat. This sits above the live
    // isr_handler stack usage, so nothing in flight is clobbered.
    uint64_t* sp = (uint64_t*)current->kstack_top;
    *--sp = 0x23;                 // ss  = user data | RPL 3
    *--sp = USER_STACK_TOP;       // rsp = user stack
    *--sp = 0x202;                // rflags (IF=1)
    *--sp = 0x1B;                 // cs  = user code | RPL 3
    *--sp = entry;                // rip = new entry point
    *--sp = 0;                    // err_code
    *--sp = 0;                    // int_no
    for (int i = 0; i < 15; i++) *--sp = 0;
    current->rsp = (uint64_t)sp;

    // The iretq in isr_common has no CR3 switch of its own, so activate the new
    // address space now. Kernel code + stack are in the shared PML4[0].
    vmm_switch(space);

    // Old space is no longer the active CR3, so it's safe to reclaim its frames.
    vmm_destroy_address_space(old_space);

    kprint("  exec ");
    kprint(nbuf);
    kprint(": ");
    kprint_uint((uint32_t)pmm_free_frame_count());
    kprint(" frames free\n");

    return current->rsp;
}

void sched_tick() { g_ticks++; }

uint64_t schedule(uint64_t rsp) {
    if (!current) return rsp;
    current->rsp = rsp;

    task* p = current;
    do {
        if (p->state == SLEEPING && p->wake_tick <= g_ticks) p->state = RUNNABLE;
        p = p->next;
    } while (p != current);

    task* n = current->next;
    while (n->state != RUNNABLE) n = n->next;
    current = n;

    tss_set_rsp0(current->kstack_top);   // kernel stack for ring3 -> ring0
    vmm_switch(current->pml4);            // switch to this task's address space
    return current->rsp;
}

void task_yield() { asm volatile("int $0x30"); }

void task_sleep(uint64_t ticks) {
    current->wake_tick = g_ticks + ticks;
    current->state     = SLEEPING;
    task_yield();
}

void sched_kill_current() { if (current) current->state = DEAD; }

void task_exit() {
    current->state = DEAD;
    task_yield();
    for (;;) { }
}
