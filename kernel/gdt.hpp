#pragma once
#include <stdint.h>

// Rebuilds the GDT with kernel + user segments and a TSS, loads it, and
// loads the task register. Replaces the minimal GDT from boot.asm.
void gdt_init();

// Sets the kernel stack the CPU switches to on a ring3 -> ring0 transition.
// (Used in step 2, once tasks run in userspace.)
void tss_set_rsp0(uint64_t rsp0);
