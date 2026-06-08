#pragma once
#include <stdint.h>

enum task_state { RUNNABLE, SLEEPING, DEAD };

struct task {
    uint64_t   rsp;
    task*      next;
    task_state state;
    uint64_t   wake_tick;
    uint64_t   kstack_top;
    uint64_t*  pml4;        // this task's address space (loaded into CR3)
};

void  sched_init();
task* task_create(void (*entry)());                                   // kernel space
task* task_create_user(uint64_t* pml4, uint64_t entry, uint64_t user_stack_top);

// Replace the current task's address space with a freshly loaded ramdisk
// program and return the rsp to resume on (a fabricated ring-3 frame), or 0 on
// failure. Intended to be called from the int 0x80 handler.
uint64_t exec_current(const char* name);

void task_yield();
void task_sleep(uint64_t ticks);
void task_exit();
void sched_kill_current();

void     sched_tick();
uint64_t schedule(uint64_t rsp);
