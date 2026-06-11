#pragma once
#include <stdint.h>

// Feed a raw set-1 scancode from the keyboard controller. Returns the ASCII
// character it produces, or 0 if the scancode was a key release, a modifier,
// or has no mapping. Tracks shift state internally across calls.
char kbd_handle_scancode(uint8_t sc);

// Input ring buffer: the IRQ1 handler pushes translated characters in,
// and SYS_READ drains them out.
void kbd_buffer_push(char c);
int kbd_getchar();            // next buffered char, or -1 if the buffer is empty