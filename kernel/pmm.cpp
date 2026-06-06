#include "pmm.hpp"
#include "serial.hpp"

// --- Multiboot2 boot information structures --------------------------------
// The info block is: u32 total_size, u32 reserved, then 8-byte-aligned tags.
struct mb_tag {
    uint32_t type;
    uint32_t size;
};

struct mb_mmap_tag {
    uint32_t type;            // 6 = memory map
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    // entries follow
};

struct mb_mmap_entry {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;            // 1 = available RAM
    uint32_t reserved;
};

extern "C" uint8_t kernel_end[];   // defined by linker.ld

static uint8_t* bitmap       = nullptr;   // one bit per frame: 1 = used
static uint64_t total_frames = 0;
static uint64_t used_frames  = 0;

static inline void bitmap_set(uint64_t f)   { bitmap[f / 8] |=  (uint8_t)(1u << (f % 8)); }
static inline void bitmap_clear(uint64_t f) { bitmap[f / 8] &= (uint8_t)~(1u << (f % 8)); }
static inline bool bitmap_test(uint64_t f)  { return bitmap[f / 8] & (1u << (f % 8)); }

static void mark_used(uint64_t addr, uint64_t len) {
    uint64_t first = addr / FRAME_SIZE;
    uint64_t last  = (addr + len + FRAME_SIZE - 1) / FRAME_SIZE;
    for (uint64_t f = first; f < last && f < total_frames; f++) {
        if (!bitmap_test(f)) { bitmap_set(f); used_frames++; }
    }
}

static void mark_free(uint64_t addr, uint64_t len) {
    uint64_t first = (addr + FRAME_SIZE - 1) / FRAME_SIZE;   // round start up
    uint64_t last  = (addr + len) / FRAME_SIZE;              // round end down
    for (uint64_t f = first; f < last && f < total_frames; f++) {
        if (bitmap_test(f)) { bitmap_clear(f); used_frames--; }
    }
}

static mb_mmap_tag* find_mmap_tag(uint64_t mb_info_addr) {
    mb_tag* tag = (mb_tag*)(mb_info_addr + 8);   // skip total_size + reserved
    while (tag->type != 0) {                     // type 0 = end tag
        if (tag->type == 6) return (mb_mmap_tag*)tag;
        tag = (mb_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7u));
    }
    return nullptr;
}

void pmm_init(uint64_t mb_info_addr) {
    mb_mmap_tag* mmap = find_mmap_tag(mb_info_addr);

    // Pass 1: find the highest available physical address.
    uint64_t highest = 0;
    if (mmap) {
        uint8_t* e   = (uint8_t*)mmap + sizeof(mb_mmap_tag);
        uint8_t* end = (uint8_t*)mmap + mmap->size;
        for (; e < end; e += mmap->entry_size) {
            mb_mmap_entry* ent = (mb_mmap_entry*)e;
            if (ent->type == 1) {
                uint64_t top = ent->base_addr + ent->length;
                if (top > highest) highest = top;
            }
        }
    }
    total_frames = highest / FRAME_SIZE;

    // Place the bitmap immediately past the kernel image, page-aligned.
    uint64_t bm = ((uint64_t)kernel_end + FRAME_SIZE - 1) & ~(FRAME_SIZE - 1);
    bitmap = (uint8_t*)bm;
    uint64_t bitmap_bytes = (total_frames + 7) / 8;

    // Start with everything marked used...
    for (uint64_t i = 0; i < bitmap_bytes; i++) bitmap[i] = 0xFF;
    used_frames = total_frames;

    // ...then free the regions the map reports as available RAM...
    if (mmap) {
        uint8_t* e   = (uint8_t*)mmap + sizeof(mb_mmap_tag);
        uint8_t* end = (uint8_t*)mmap + mmap->size;
        for (; e < end; e += mmap->entry_size) {
            mb_mmap_entry* ent = (mb_mmap_entry*)e;
            if (ent->type == 1) mark_free(ent->base_addr, ent->length);
        }
    }

    // ...and re-reserve what we must never hand out:
    mark_used(0, 0x100000);                                   // low 1 MiB
    mark_used(0x200000, (uint64_t)kernel_end - 0x200000);     // kernel image
    mark_used(bm, bitmap_bytes);                              // the bitmap itself
}

void* pmm_alloc_frame() {
    for (uint64_t f = 0; f < total_frames; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames++;
            return (void*)(f * FRAME_SIZE);
        }
    }
    return nullptr;   // out of physical memory
}

void pmm_free_frame(void* frame) {
    uint64_t f = (uint64_t)frame / FRAME_SIZE;
    if (f < total_frames && bitmap_test(f)) {
        bitmap_clear(f);
        used_frames--;
    }
}

uint64_t pmm_free_frame_count()  { return total_frames - used_frames; }
uint64_t pmm_total_frame_count() { return total_frames; }