#pragma once
#include <stdint.h>

void serial_init();
void kprint(const char* str);
void kprint_uint(uint32_t n);
void kprint_hex(uint32_t n);
void kprint_char(char c);
void kprint_ptr(void* p);