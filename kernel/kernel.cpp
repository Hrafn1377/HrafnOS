#include "serial.hpp"

extern "C" void kmain() {
    kprint("HrafnOS booting...\n");
    kprint("Memory: assuming 128MB available at 0x0\n");
    kprint("Kernel loaded OK.\n");
    while (true) asm volatile("hlt");
}