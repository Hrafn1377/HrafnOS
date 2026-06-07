#pragma once
#include <stdint.h>

// A trivial read-only "filesystem": a static table mapping a name to the
// embedded ELF bytes for that program.
struct ramdisk_entry {
    const char* name;
    uint8_t*    start;
    uint8_t*    end;
};

uint8_t*             ramdisk_lookup(const char* name);  // ELF bytes, or nullptr
uint64_t             ramdisk_size(const char* name);    // byte length, or 0
uint32_t             ramdisk_count();
const ramdisk_entry* ramdisk_get(uint32_t i);           // nullptr if out of range
