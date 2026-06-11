#pragma once
#include <stdint.h>

// Feed a raw set-1 scancode from the keyboard controller. Returns the ASCII
// character it produces, or 0 if the scancode was a key release, a modifier,
// or has no mapping. Tracks shift state internally across calls.
char kbd_handle_scancode(uint8_t sc);