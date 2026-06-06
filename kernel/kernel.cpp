#include "serial.hpp"
#include "heap.hpp"

extern "C" void kmain() {
    serial_init();                       // initialize COM1 before any output
    kprint("HrafnOS booting...\n");

    heap_init();
    void* raw = kmalloc(4);
    kprint("raw: ");
    kprint_ptr(raw);                     // full 64-bit pointer, no truncation
    kprint_char('\n');

    while (true) asm volatile("hlt");
}