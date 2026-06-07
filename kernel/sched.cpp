#include "sched.hpp"
#include "heap.hpp"
#include "gdt.hpp"

#define STACK_SIZE 16384

static task*             current = nullptr;
static volatile uint64_t g_ticks = 0;

void sched_init() {
    task* boot = (task*)kmalloc(sizeof(task));
    boot->next       = boot;
    boot->state      = RUNNABLE;
    boot->wake_tick  = 0;
    boot->kstack_top = 0;          // idle runs in ring 0; RSP0 never used for it
    current = boot;
}

task* task_create(void (*entry)()) {
    task*    t         = (task*)kmalloc(sizeof(task));
    uint64_t stack     = (uint64_t)kmalloc(STACK_SIZE);
    uint64_t stack_top = (stack + STACK_SIZE) & ~15ULL;

    uint64_t* sp = (uint64_t*)stack_top;
    *--sp = 0x10;                 // ss  (kernel data)
    *--sp = stack_top - 8;        // rsp
    *--sp = 0x202;                // rflags (IF=1)
    *--sp = 0x08;                 // cs  (kernel code)
    *--sp = (uint64_t)entry;      // rip
    *--sp = 0;                    // err_code
    *--sp = 0;                    // int_no
    for (int i = 0; i < 15; i++) *--sp = 0;

    t->rsp        = (uint64_t)sp;
    t->state      = RUNNABLE;
    t->wake_tick  = 0;
    t->kstack_top = stack_top;    // ring0 task: its own stack
    t->next       = current->next;
    current->next = t;
    return t;
}

task* task_create_user(uint64_t entry, uint64_t user_stack_top) {
    task*    t          = (task*)kmalloc(sizeof(task));
    uint64_t kstack     = (uint64_t)kmalloc(STACK_SIZE);
    uint64_t kstack_top = (kstack + STACK_SIZE) & ~15ULL;

    // Fabricate a frame that iretq will drop into ring 3: CS/SS carry RPL 3.
    uint64_t* sp = (uint64_t*)kstack_top;
    *--sp = 0x23;                 // ss  = user data (0x20) | RPL 3
    *--sp = user_stack_top;       // rsp = user stack
    *--sp = 0x202;                // rflags (IF=1)
    *--sp = 0x1B;                 // cs  = user code (0x18) | RPL 3
    *--sp = entry;                // rip = user entry point
    *--sp = 0;                    // err_code
    *--sp = 0;                    // int_no
    for (int i = 0; i < 15; i++) *--sp = 0;

    t->rsp        = (uint64_t)sp;
    t->state      = RUNNABLE;
    t->wake_tick  = 0;
    t->kstack_top = kstack_top;   // RSP0: where this task's ring3->0 frames land
    t->next       = current->next;
    current->next = t;
    return t;
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

    tss_set_rsp0(current->kstack_top);   // CPU loads this on the next ring3->0
    return current->rsp;
}

void sched_kill_current() {
    if (current) current->state = DEAD;
}

void task_yield() { asm volatile("int $0x30"); }

void task_sleep(uint64_t ticks) {
    current->wake_tick = g_ticks + ticks;
    current->state     = SLEEPING;
    task_yield();
}

void task_exit() {
    current->state = DEAD;
    task_yield();
    for (;;) { }
}
