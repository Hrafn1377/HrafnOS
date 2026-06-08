#pragma once
#include <stdint.h>

// Layout of a user process's private address space (PML4[1]).
#define USER_STACK_TOP  0x8000100000ULL   // top of the user stack (exclusive)
#define USER_STACK_SIZE 0x4000ULL         // 16 KiB
