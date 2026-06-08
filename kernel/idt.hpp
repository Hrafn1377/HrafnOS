#pragma once
#include <stdint.h>

// The saved register frame, in the exact order isr_common pushes/pops it.
// (Shared with the scheduler so fork can duplicate a task's frame.)
struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no;
    uint64_t err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

// Installs the IDT with handlers for CPU exceptions (vector 0-31)
// and loads it with lidt.
void idt_init();