#include "heap.hpp"
#include "serial.hpp"

void heap_init(Heap* h) {
    h->next = 0x1000000ULL;  // 16MB
    h->end  = 0x2000000ULL;  // 32MB
}

__attribute__((noinline, optimize("O0"))) void* kmalloc(Heap* h, uint32_t size) {
    volatile uint64_t* next_ptr = &h->next;
    uint64_t cur = *next_ptr;
    if (size == 0) return nullptr;
    size = (size + 7) & ~7;
    uint64_t nxt = cur + (uint64_t)size;
    if (nxt > h->end) {
        kprint("oom\n");
        return nullptr;
    }
    *next_ptr = nxt;
    return (void*)cur;
}