#include "serial.hpp"
#include "console.hpp"

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init() {
    outb(0x3F9, 0x00);   // disable interrupts
    outb(0x3FB, 0x80);   // enable DLAB
    outb(0x3F8, 0x03);   // divisor low  -> 38400 baud
    outb(0x3F9, 0x00);   // divisor high
    outb(0x3FB, 0x03);   // 8N1, clear DLAB
    outb(0x3FA, 0xC7);   // enable FIFO, clear, 14-byte threshold
    outb(0x3FC, 0x0B);   // DTR, RTS, OUT2
}

void kprint_char(char c) {
    while (!(inb(0x3FD) & 0x20)) { }   // wait for transmit holding reg empty
    outb(0x3F8, c);
    if (console_ready()) console_putchar(c);       // mirror to the screen once it's up
}

void kprint(const char* str) {
    for (int i = 0; str[i] != 0; i++) kprint_char(str[i]);
}

int serial_getchar() {
    if (inb(0x3FD) & 0x01) return inb(0x3F8);   // LSR bit 0 = data ready
    return -1;
}

void kprint_uint(uint32_t n) {
    if (n == 0) { kprint_char('0'); return; }
    uint8_t d;
    bool s = false;
    #define DIGIT(p) \
        d = 0; \
        while (n >= p) { n -= p; d++; } \
        if (d || s) { s = true; kprint_char('0' + d); }
    DIGIT(1000000000) DIGIT(100000000) DIGIT(10000000) DIGIT(1000000)
    DIGIT(100000) DIGIT(10000) DIGIT(1000) DIGIT(100) DIGIT(10)
    #undef DIGIT
    kprint_char('0' + n);
}

void kprint_hex(uint32_t n) {
    kprint("0x");
    bool leading = true;
    for (int i = 7; i >= 0; i--) {
        uint32_t nibble = (n >> (i * 4)) & 0xFU;
        if (nibble != 0) leading = false;
        if (!leading) {
            if (nibble < 10) kprint_char('0' + nibble);
            else kprint_char('A' + nibble - 10);
        }
    }
    if (leading) kprint_char('0');
}

void kprint_ptr(void* p) {
    uint64_t addr = (uint64_t)p;
    kprint("0x");
    for (int i = 15; i >= 0; i--) {
        uint32_t nibble = (uint32_t)((addr >> (i * 4)) & 0xF);
        if (nibble < 10) kprint_char('0' + nibble);
        else kprint_char('A' + nibble - 10);
    }
}
