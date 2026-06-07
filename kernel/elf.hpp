#pragma once
#include <stdint.h>

// Loads an ELF64 executable into a fresh address space. On success returns the
// new address space (PML4) and writes the entry point to *entry_out; returns
// nullptr on a malformed/unsupported image.
uint64_t* elf_load(uint8_t* elf_data, uint64_t* entry_out);
