#include "serial.hpp"

extern "C" void kmain() {
    kprint("HrafnOS is alive!\n");
    while (true) asm volatile("hlt");
}