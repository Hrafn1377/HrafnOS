#include "sched.hpp"
#include "heap.hpp"

#define STACK_SIZE 16384

static task*            current = nullptr;
static volatile uint64_t g_ticks = 0;

void sched_init() {
    task* boot = (task*)kmalloc(sizeof(task));
    boot->next      = boot;          // ring of one
    boot->state     = RUNNABLE;      // the idle task is always runnable
    boot->wake_tick = 0;
    current = boot;
}

task* task_create(void (*entry)()) {
    task*    t         = (task*)kmalloc(sizeof(task));
    uint64_t stack     = (uint64_t)kmalloc(STACK_SIZE);
    uint64_t stack_top = (stack + STACK_SIZE) & ~15ULL;

    uint64_t* sp = (uint64_t*)stack_top;
    *--sp = 0x10;                 // ss
    *--sp = stack_top - 8;        // rsp
    *--sp = 0x202;                // rflags: IF=1
    *--sp = 0x08;                 // cs
    *--sp = (uint64_t)entry;      // rip
    *--sp = 0;                    // err_code
    *--sp = 0;                    // int_no
    for (int i = 0; i < 15; i++) *--sp = 0;   // general-purpose registers

    t->rsp       = (uint64_t)sp;
    t->state     = RUNNABLE;
    t->wake_tick = 0;
    t->next      = current->next;
    current->next = t;
    return t;
}

void sched_tick() { g_ticks++; }

uint64_t schedule(uint64_t rsp) {
    if (!current) return rsp;
    current->rsp = rsp;                       // save outgoing task

    // Wake any sleeper whose time has arrived (full-ring scan).
    task* p = current;
    do {
        if (p->state == SLEEPING && p->wake_tick <= g_ticks) p->state = RUNNABLE;
        p = p->next;
    } while (p != current);

    // Pick the next RUNNABLE task. The idle task is always runnable, so this
    // loop always terminates.
    task* n = current->next;
    while (n->state != RUNNABLE) n = n->next;
    current = n;
    return current->rsp;
}

void task_yield() {
    asm volatile("int $0x30");                // software reschedule (vector 48)
}

void task_sleep(uint64_t ticks) {
    current->wake_tick = g_ticks + ticks;
    current->state     = SLEEPING;
    task_yield();                             // scheduler skips us until woken
}

void task_exit() {
    current->state = DEAD;                    // scheduler will never pick us again
    task_yield();
    for (;;) { }                              // unreachable
}