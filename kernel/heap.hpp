#pragma once
#include <stdint.h>

// A free-list kernel heap backed by the PMM (frames) and VMM (mappings).
void heap_init();
void* kmalloc(uint64_t size);
void kfree(void* ptr);