#include "heap.hpp"

#define HEAP_START 0x2000000ULL
#define HEAP_END   0x7000000ULL

extern "C" uint64_t asm_get_heap();
extern "C" void     asm_set_heap(uint64_t val);

void heap_init() {
    asm_set_heap(HEAP_START);
}

void* kmalloc(uint32_t size) {
    if (size == 0) return nullptr;
    size = (size + 7) & ~7U;                 // round up to 8-byte alignment
    uint64_t cur = asm_get_heap();
    uint64_t nxt = cur + (uint64_t)size;
    if (nxt > HEAP_END) return nullptr;      // out of heap
    asm_set_heap(nxt);
    return (void*)cur;
}