#include "sched.hpp"
#include "heap.hpp"
#include "gdt.hpp"
#include "vmm.hpp"
#include "pmm.hpp"
#include "elf.hpp"
#include "ramdisk.hpp"
#include "userspace.hpp"
#include "serial.hpp"
#include "idt.hpp"

#define STACK_SIZE 16384
#define MAX_ARGS   16
#define MAX_ARG_LEN 64  

static task*             current = nullptr;
static volatile uint64_t g_ticks = 0;

void sched_init() {
    task* boot = (task*)kmalloc(sizeof(task));
    boot->next       = boot;
    boot->state      = RUNNABLE;
    boot->wake_tick  = 0;
    boot->kstack_top = 0;
    boot->kstack_base = 0;
    boot->pml4       = vmm_kernel_space();   // idle runs in the kernel space
    boot->parent     = nullptr;
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
    t->kstack_base = stack;
    t->pml4       = vmm_kernel_space();
    t->parent     = nullptr;
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
    t->kstack_base = kstack;
    t->pml4       = pml4;         // this process's private address space
    t->parent     = nullptr;     // spawned by the kernel, no parent
    t->next       = current->next;
    current->next = t;
    return t;
}

uint64_t exec_current(char** argv, int argc) {
    // Copy argv out of user memory now, while the caller's address space is still
    // active. After the CR3 switch below, those user addresses point at the new
    // program's memory -- so we stash everything kernel-side first.
    static char argbuf[MAX_ARGS][MAX_ARG_LEN];
    if (argc < 0)        argc = 0;
    if (argc > MAX_ARGS) argc = MAX_ARGS;
    for (int i = 0; i < argc; i++) {
        const char* src = argv[i];
        int j = 0;
        while (src[j] && j < MAX_ARG_LEN - 1) { argbuf[i][j] = src[j]; j++; }
        argbuf[i][j] = 0;
    }

    uint8_t* elf = ramdisk_lookup(argbuf[0]);   // argv[0] is the program name
    if (!elf) return 0;

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

    // Activate the new space so we can write the argv block onto its user stack.
    vmm_switch(space);

    // Lay out argv on the new stack: strings from the top down, then a
    // NULL-terminated pointer array, then a 16-aligned entry rsp below it.
    uint64_t sp = USER_STACK_TOP;
    char* uarg[MAX_ARGS];
    for (int i = argc - 1; i >= 0; i--) {
        int len = 0;
        while (argbuf[i][len]) len++;
        len++;                                  // include the NUL
        sp -= (uint64_t)len;
        for (int b = 0; b < len; b++) ((char*)sp)[b] = argbuf[i][b];
        uarg[i] = (char*)sp;
    }
    sp &= ~0xFULL;
    sp -= (uint64_t)(argc + 1) * 8;             // argc pointers + a NULL terminator
    char** uargv = (char**)sp;
    for (int i = 0; i < argc; i++) uargv[i] = uarg[i];
    uargv[argc] = nullptr;
    sp &= ~0xFULL;                              // 16-align the entry rsp

    // Fabricate a fresh ring-3 entry frame, passing argc/argv in rdi/rsi.
    registers* frame = (registers*)(current->kstack_top - sizeof(registers));
    for (uint64_t i = 0; i < sizeof(registers) / 8; i++) ((uint64_t*)frame)[i] = 0;
    frame->rip    = entry;
    frame->cs     = 0x1B;                       // user code | RPL 3
    frame->rflags = 0x202;                      // IF = 1
    frame->rsp    = sp;
    frame->ss     = 0x23;                       // user data | RPL 3
    frame->rdi    = (uint64_t)argc;             // _start(int argc, ...)
    frame->rsi    = (uint64_t)uargv;            // _start(..., char** argv)
    current->rsp  = (uint64_t)frame;

    // Old space is no longer the active CR3, so it's safe to reclaim its frames.
    vmm_destroy_address_space(old_space);
    return current->rsp;
}

static uint64_t next_pid = 1;

int fork_current(registers* parent) {
    // Clone the parent's address space (deep copy of every user page).
    uint64_t* child_space = vmm_clone_address_space(current->pml4);
    if (!child_space) return -1;

    // New task with its own kernel stack.
    task*    t          = (task*)kmalloc(sizeof(task));
    uint64_t kstack     = (uint64_t)kmalloc(STACK_SIZE);
    uint64_t kstack_top = (kstack + STACK_SIZE) & ~15ULL;

    // Drop a copy of the parent's syscall frame at the top of the child's kernel
    // stack. When the scheduler resumes the child, isr_common pops this frame and
    // iretq's back into the child's (cloned) user code at the same rip -- but with
    // rax = 0, which is what fork() returns in the child.
    registers* child_frame = (registers*)(kstack_top - sizeof(registers));
    *child_frame = *parent;
    child_frame->rax = 0;

    t->rsp        = (uint64_t)child_frame;
    t->state      = RUNNABLE;
    t->wake_tick  = 0;
    t->kstack_top = kstack_top;
    t->kstack_base = kstack;
    t->pml4       = child_space;
    t->parent     = current;       // for wait()
    t->next       = current->next;
    current->next = t;

    return (int)next_pid++;   // parent's fork() return value
}

uint64_t wait_current(uint64_t rsp) {
    current->rsp = rsp;

    // Any still-running child to wait for?
    bool  live = false;
    task* t    = current->next;
    while (t != current) {
        if (t->parent == current && t->state != DEAD) { live = true; break; }
        t = t->next;
    }

    ((registers*)rsp)->rax = 0;          // wait() returns 0

    if (!live) return rsp;               // nothing to wait for; return immediately
    current->state = WAITING;            // park until the reaper wakes us
    return schedule(rsp);
}

void sched_tick() { g_ticks++; }

// Reclaim tasks that have exited. Never touches `current` -- we're running on its
// kernel stack and in its address space -- so a just-exited task (still current
// when it died) is reaped on a later schedule, once some other task is current.
static void reap_dead() {
    task* prev = current;
    task* t    = current->next;
    while (t != current) {
        if (t->state == DEAD) {
            prev->next = t->next;       // unlink from the ring
            task* dead = t;
            t = t->next;
            if (dead->parent && dead->parent->state == WAITING)
                dead->parent->state = RUNNABLE;     // wake a parent blocked in wait()
            vmm_destroy_address_space(dead->pml4); // not the active CR3 (not current)
            kfree((void*)dead->kstack_base);       // its kernel stack
            kfree(dead);                           // its task struct
        } else {
            prev = t;
            t = t->next;
        }
    }
}


uint64_t schedule(uint64_t rsp) {
    if (!current) return rsp;
    current->rsp = rsp;
    reap_dead();

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
