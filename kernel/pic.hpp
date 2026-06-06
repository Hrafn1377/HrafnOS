#pragma once
#include <stdint.h>

// Remaps the 8259 PIC pair so IRQ0-15 land on vectors 32-47,
// clear of the CPU exception vectors (0-31).
void pic_remap();

// Signals enf-of-interupt to the PIC(s) for the given IRQ line.
void pic_send_eoi(uint8_t irq);

// Programs PIT channel 0 to fire IRQ0 at the given frequency (Hz).
void pit_init(uint32_t hz);