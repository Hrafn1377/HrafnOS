#pragma once
#include <stdint.h>

// A drawable surface: 32-bit pixels (0x00RRGGBB), row-major
// `pitch` is the row stride in PIXELS (not bytes).
struct Surface {
    uint32_t* pixels;
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;
};

#define RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

void gfx_clear      (Surface* s, uint32_t color);
void gfx_putpixel   (Surface* s, int x, int y, uint32_t color);
void gfx_fill_rect  (Surface* s, int x, int y, int w, int h, uint32_t color);
void gfx_rect       (Surface* s, int x, int y, int w, int h, uint32_t color);   // 1px outline
void gfx_hline      (Surface* s, int x, int y, int w, uint32_t color);
void gfx_vline      (Surface* s, int x, int y, int h, uint32_t color);
void gfx_line       (Surface* s, int x0, int y0, int x1, int y1, uint32_t color);

// A Surface wrapping the hardware framebuffer.
Surface gfx_screen();
Surface gfx_backbuffer();    // off-screen Surface to draw into
void    gfx_present();       // blit the back buffer to the screen
void gfx_cursor(Surface* s, int x, int y);