#pragma once
#include <stdint.h>

// Set up a text console over the framebuffer. Call after fb_init() succeeds.
void console_init();

// Write one character. Handles '\n' and '\r', draws everything else as a
// glyph, and wraps as the right edge.
void console_putchar(char c);

// Write a NUL-terminated string.
void console_write(const char* s);

// True once console_init() has run; lets kprint mirror to the screen
bool console_ready();