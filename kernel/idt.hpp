#pragma once
#include <stdint.h>

// Installs the IDT with handlers for CPU exceptions (vectors 0–31)
// and loads it with lidt.
void idt_init();