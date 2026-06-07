#pragma once
#include <stdint.h>

// Syscall numbers (passed in rax; args in rdi, rsi, ... like the SysV/Linux ABI).
#define SYS_WRITE 0
#define SYS_YIELD 1

static inline long syscall0(long num) {
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num) : "memory");
    return ret;
}

static inline long syscall1(long num, long a1) {
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "D"(a1) : "memory");
    return ret;
}

static inline void sys_write(const char* s) { syscall1(SYS_WRITE, (long)s); }
static inline void sys_yield()              { syscall0(SYS_YIELD); }
