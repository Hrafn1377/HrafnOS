#pragma once
#include <stdint.h>

void    mouse_init();   // 8042 aux setup + enable streaming + unmask IRQ12
void    mouse_irq();    // called from isr_handler on IRQ12 (vector 44)
int     mouse_x();
int     mouse_y();
uint8_t mouse_buttons(); // bit0 left, bit1 right, bit2 middle