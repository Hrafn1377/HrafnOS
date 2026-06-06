#pragma once
#include <stdint.h>

#define FRAME_SIZE 4096ULL

// Parses the Multiboot2 memory map and builds a frame bitmap.
void pmm_init(uint64_t mb_info_addr);

// Allocates one 4 KiB physical frame. Returns its physical address,
// or nullptr if no frames remain.
void* pmm_alloc_frame();

// Returns a frame to the pool.
void pmm_free_frame(void* frame);

uint64_t pmm_free_frame_count();
uint64_t pmm_total_frame_count();