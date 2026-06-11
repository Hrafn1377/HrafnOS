#pragma once
#include <stdint.h>

constexpr uint32_t FONT_W = 8;
constexpr uint32_t FONT_H = 16;

// Pointer to the 16-byte bitmap for character c (printable ASCII; anything
// out of range falls back to '?'). One byte per row, bit 0x80 = leftmost pixel.
const uint8_t* font_glyph(uint8_t c);