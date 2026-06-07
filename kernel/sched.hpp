#pragma once
#include <stdint.h>

enum task_state { RUNNABLE, SLEEPING, DEAD };

struct task {
    uint64_t   rsp;
    task*      next;
    task_state state;
    uint64_t   wake_tick;
    uint64_t   kstack_top;
};

void  sched_init();
task* task_create(void (*entry)());
task* task_create_user(uint64_t entry, uint64_t user_stack_top);

void task_yield();
void task_sleep(uint64_t ticks);
void task_exit();

void sched_kill_current();      // mark the running task DEAD (for SYS_EXIT)

void     sched_tick();
uint64_t schedule(uint64_t rsp);
