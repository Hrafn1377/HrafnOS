#pragma once
#include <stdint.h>

enum task_state { RUNNABLE, SLEEPING, DEAD };

struct task {
    uint64_t   rsp;        // saved kernel stack pointer (points at a frame)
    task*      next;       // round-robin ring
    task_state state;
    uint64_t   wake_tick;  // for SLEEPING tasks: g_ticks value to wake at
};

void  sched_init();
task* task_create(void (*entry)());

// Task lifecycle (call from within a running task):
void task_yield();              // give up the CPU now
void task_sleep(uint64_t ticks);// block for N timer ticks
void task_exit();               // terminate; never returns

// Called from the timer IRQ.
void     sched_tick();          // advance the tick clock
uint64_t schedule(uint64_t rsp);// pick next task; returns its stack pointer