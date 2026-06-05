#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

extern "C" void kmain() {
    outb(0x3F8, 'H');
    outb(0x3F8, 'r');
    outb(0x3F8, 'a');
    outb(0x3F8, 'f');
    outb(0x3F8, 'n');
    outb(0x3F8, 'O');
    outb(0x3F8, 'S');
    outb(0x3F8, ' ');
    outb(0x3F8, 'i');
    outb(0x3F8, 's');
    outb(0x3F8, ' ');
    outb(0x3F8, 'a');
    outb(0x3F8, 'l');
    outb(0x3F8, 'i');
    outb(0x3F8, 'v');
    outb(0x3F8, 'e');
    outb(0x3F8, '!');
    outb(0x3F8, '\n');
    while (true) asm volatile("hlt");
}