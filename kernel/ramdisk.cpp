#include "ramdisk.hpp"

extern "C" uint8_t prog_one_start[];
extern "C" uint8_t prog_one_end[];
extern "C" uint8_t prog_two_start[];
extern "C" uint8_t prog_two_end[];
extern "C" uint8_t prog_shell_start[];
extern "C" uint8_t prog_shell_end[];
extern "C" uint8_t prog_help_start[];
extern "C" uint8_t prog_help_end[];
extern "C" uint8_t prog_clear_start[];
extern "C" uint8_t prog_clear_end[];
 
static ramdisk_entry entries[] = {
    { "one", prog_one_start, prog_one_end },
    { "two", prog_two_start, prog_two_end },
    { "shell", prog_shell_start, prog_shell_end },
    { "help", prog_help_start, prog_help_end },
    { "clear", prog_clear_start, prog_clear_end },
};
static const uint32_t N = sizeof(entries) / sizeof(entries[0]);

static bool streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return false;
        a++; b++;
    }
    return *a == *b;
}

uint8_t* ramdisk_lookup(const char* name) {
    for (uint32_t i = 0; i < N; i++)
        if (streq(entries[i].name, name)) return entries[i].start;
    return nullptr;
}

uint64_t ramdisk_size(const char* name) {
    for (uint32_t i = 0; i < N; i++)
        if (streq(entries[i].name, name))
            return (uint64_t)(entries[i].end - entries[i].start);
    return 0;
}

uint32_t ramdisk_count() { return N; }

const ramdisk_entry* ramdisk_get(uint32_t i) {
    return (i < N) ? &entries[i] : nullptr;
}
