#include "gfx.hpp"
#include "fb.hpp"

void gfx_putpixel(Surface* s, int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= (int)s->width || y >= (int)s->height) return;
    s->pixels[(uint64_t)y * s->pitch + (uint32_t)x] = color;
}

void gfx_clear(Surface* s, uint32_t color) {
    for (uint32_t y = 0; y < s->height; y++) {
        uint32_t* row = s->pixels + (uint64_t)y * s->pitch;
        for (uint32_t x = 0; x < s->width; x++) row[x] = color;
    }
}

void gfx_fill_rect(Surface* s, int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)s->width)  x1 = (int)s->width;
    if (y1 > (int)s->height) y1 = (int)s->height;
    for (int yy = y0; yy < y1; yy++) {
        uint32_t* row = s->pixels + (uint64_t)yy * s-> pitch;
        for (int xx = x0; xx < x1; xx++) row[xx] = color;
    }
}

void gfx_hline(Surface* s, int x, int y, int w, uint32_t color) { gfx_fill_rect(s, x, y, w, 1, color); }
void gfx_vline(Surface* s, int x, int y, int h, uint32_t color) { gfx_fill_rect(s, x, y, 1, h, color); }

void gfx_rect(Surface* s, int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    gfx_hline(s, x, y,         w, color);
    gfx_hline(s, x, y + h - 1, w, color);
    gfx_vline(s, x, y,         h, color);
    gfx_vline(s, x + w - 1, y, h, color);
}

void gfx_line(Surface* s, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = x1 - x0; if (dx < 0) dx = -dx;
    int dy = y1 - y0; if (dy < 0) dy = -dy;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        gfx_putpixel(s, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

Surface gfx_screen() {
    Surface s;
    s.pixels = (uint32_t*)fb_base();
    s.width  = fb_width();
    s.height = fb_height();
    s.pitch  = fb_pitch() / 4;   // bytes -> pixels
    return s;
}

// ---- double buffering (step 2) ----
// Fixed max geometry; the active screen (<= this) is what we present
#define GFX_BB_W 1024
#define GFX_BB_H 768
static uint32_t g_backbuf[GFX_BB_W * GFX_BB_H];     // 3MiB, in BSS (identity-mapped)

Surface gfx_backbuffer() {
    Surface s;
    s.pixels = g_backbuf;
    s.width  = fb_width();  // visible region (clipping target)
    s.height = fb_height();
    s.pitch  = GFX_BB_W;    // fixed row stride
    return s;
}

// Blit the back buffer to the hardware framebuffer.
void gfx_present() {
    volatile uint8_t* fb = fb_base();
    if (!fb) return;
    uint32_t pitch = fb_pitch();
    uint32_t w = fb_width();
    uint32_t h = fb_height();
    for (uint32_t y = 0; y < h; y++) {
        volatile uint32_t* dst = (volatile uint32_t*)(fb + (uint64_t)y * pitch);
        uint32_t*          src = g_backbuf + (uint64_t)y * GFX_BB_W;
        for (uint32_t x = 0; x < w; x++) dst[x] = src[x];
    }
}