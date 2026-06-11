#pragma once
#include <stdint.h>

// Parse GRUB's Multiboot2 framebuffer tag, map the framebuffer into kernel
// space, and record its geometry. Returns false if no usable framebuffer.
bool fb_init(uint64_t mb_info_addr);

// Fill the whole screen with one color (0X00RRGGBB).
void fb_fill(uint32_t color);

// Plot a single pixel (bounds-checked).
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);

// Shift the framebuffer up by `lines` scanlines; fill the freed bottom
// region with `color`. Used by the console to scroll.
void fb_scroll_up(uint32_t lines, uint32_t color);
uint32_t fb_width();
uint32_t fb_height();
bool     fb_ready();