#include "serial.hpp"

extern "C" void kmain() {
    kprint("HrafnOS is alive!\n");
    kprint_uint(42);
    kprint_char('\n');
    kprint_hex(0xDEADBEEF);
    kprint_char('\n');
    while (true) asm volatile("hlt");
}