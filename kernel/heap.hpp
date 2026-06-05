#pragma once
#include <stdint.h>

struct Heap {
    uint64_t next;
    uint64_t end;
};

void heap_init(Heap* h);
void* kmalloc(Heap* h, uint32_t size);