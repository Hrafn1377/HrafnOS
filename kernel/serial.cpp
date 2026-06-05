#include "serial.hpp"

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init() {
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x80);
    outb(0x3F8, 0x03);
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x03);
    outb(0x3FA, 0xC7);
    outb(0x3FC, 0x0B);
}

void kprint_char(char c) {
    outb(0x3F8, c);
}

void kprint(const char* str) {
    const volatile char* s = str;
    for (int i = 0; s[i] != 0; i++) {
        kprint_char(s[i]);
    }
}

__attribute__((noinline, optimize("O0"))) void kprint_uint(uint32_t n) {
    if (n == 0) { kprint_char('0'); return; }

    uint8_t d;
    bool s = false;

    #define DIGIT(p) \
        d = 0; \
        while (n >= p) { n -= p; d++; } \
        if (d || s) { s = true; kprint_char('0' + d); }

    DIGIT(1000000000)
    DIGIT(100000000)
    DIGIT(10000000)
    DIGIT(1000000)
    DIGIT(100000)
    DIGIT(10000)
    DIGIT(1000)
    DIGIT(100)
    DIGIT(10)

    #undef DIGIT

    kprint_char('0' + n);
}

__attribute__((noinline, optimize("O0"))) void kprint_hex(uint32_t n) {
    kprint("0x");
    bool leading = true;
    for (int i = 7; i >= 0; i--) {
        uint8_t nibble = (n >> (i * 4)) & 0xF;
        if (nibble != 0) leading = false;
        if (!leading) {
            if (nibble < 10) kprint_char('0' + nibble);
            else kprint_char('A' + nibble - 10);
        }
    }
    if (leading) kprint_char('0');
}