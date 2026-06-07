#pragma once
#include <stdint.h>

enum task_state { RUNNABLE, SLEEPING, DEAD };

struct task {
    uint64_t   rsp;        // saved kernel stack pointer (points at a frame)
    task*      next;       // round-robin ring
    task_state state;
    uint64_t   wake_tick;  // for SLEEPING tasks
    uint64_t   kstack_top; // TSS.RSP0 for this task (ring3 -> ring0 landing stack)
};

void  sched_init();
task* task_create(void (*entry)());                          // ring 0 task
task* task_create_user(uint64_t entry, uint64_t user_stack_top); // ring 3 task

void task_yield();
void task_sleep(uint64_t ticks);
void task_exit();

void     sched_tick();
uint64_t schedule(uint64_t rsp);
