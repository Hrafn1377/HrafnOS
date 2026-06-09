#pragma once
#include <stdint.h>

struct registers;      // defined in idt.hpp

enum task_state { RUNNABLE, SLEEPING, WAITING, DEAD };

struct task {
    uint64_t   rsp;
    task*      next;
    task*      parent;
    task_state state;
    uint64_t   wake_tick;
    uint64_t   kstack_top;
    uint64_t   kstack_base;
    uint64_t*  pml4;        // this task's address space (loaded into CR3)

};

void  sched_init();
task* task_create(void (*entry)());                                   // kernel space
task* task_create_user(uint64_t* pml4, uint64_t entry, uint64_t user_stack_top);

// Replace the current task's address space with a freshly loaded ramdisk
// program and return the rsp to resume on (a fabricated ring-3 frame), or 0 on
// failure. Intended to be called from the int 0x80 handler.
uint64_t exec_current(const char* name);

// Fork the current task: clone its address space and register frame into a new
// RUNNABLE task that resumes from the same syscall with rax = 0. Returns the
// child's id to the parent, or -1 on failure.
int fork_current(registers* parent);

// Block the current task until one of its children exits (is reaped). Returns
// the rsp to resume on. If it has no live children, returns immediately.
uint64_t wait_current(uint64_t rsp);

void task_yield();
void task_sleep(uint64_t ticks);
void task_exit();
void sched_kill_current();

void     sched_tick();
uint64_t schedule(uint64_t rsp);
