#include "heap.hpp"
#include "pmm.hpp"
#include "vmm.hpp"

#define HEAP_BASE 0x100000000ULL    // 4 GiB — clear of the 1 GiB identity region
#define HEAP_SIZE 0x200000ULL       // 2 MiB arena

// Each allocation is preceded by this header. Blocks are kept in a single
// address-ordered list; because a block's payload is immediately followed by
// the next block's header, list order == physical order, which makes
// coalescing adjacent free blocks straightforward.
struct block_header {
    uint64_t      size;    // payload size in bytes (excludes this header)
    block_header* next;    // next block, in address order
    bool          free;
};

static block_header* head = nullptr;

void heap_init() {
    // Back the arena with real memory: one PMM frame per page, mapped by the VMM.
    for (uint64_t off = 0; off < HEAP_SIZE; off += FRAME_SIZE) {
        void* frame = pmm_alloc_frame();
        vmm_map_page(HEAP_BASE + off, (uint64_t)frame, PAGE_PRESENT | PAGE_WRITABLE);
    }

    // Start with one free block spanning the whole arena.
    head = (block_header*)HEAP_BASE;
    head->size = HEAP_SIZE - sizeof(block_header);
    head->next = nullptr;
    head->free = true;
}

void* kmalloc(uint64_t size) {
    if (size == 0) return nullptr;
    size = (size + 7) & ~7ULL;                       // 8-byte align the request

    for (block_header* b = head; b != nullptr; b = b->next) {
        if (b->free && b->size >= size) {
            // Split only if the remainder can hold a header plus useful payload.
            if (b->size >= size + sizeof(block_header) + 16) {
                block_header* split =
                    (block_header*)((uint8_t*)b + sizeof(block_header) + size);
                split->size = b->size - size - sizeof(block_header);
                split->next = b->next;
                split->free = true;
                b->next = split;
                b->size = size;
            }
            b->free = false;
            return (uint8_t*)b + sizeof(block_header);
        }
    }
    return nullptr;   // arena exhausted
}

// Merge adjacent free blocks so freed memory becomes reusable in large chunks.
static void coalesce() {
    for (block_header* b = head; b && b->next; ) {
        if (b->free && b->next->free) {
            b->size += sizeof(block_header) + b->next->size;
            b->next  = b->next->next;             // re-check against the new next
        } else {
            b = b->next;
        }
    }
}

void kfree(void* ptr) {
    if (!ptr) return;
    block_header* b = (block_header*)((uint8_t*)ptr - sizeof(block_header));
    b->free = true;
    coalesce();
}