#include "console.hpp"
#include "font.hpp"
#include "fb.hpp"

static uint32_t g_cols = 0;            //screen width in characters
static uint32_t g_rows = 0;            // screen height in characters
static uint32_t g_col  = 0;            // cursor column
static uint32_t g_row  = 0;            // sursor row
static uint32_t g_fg   = 0x00FFFFFF;   // foreground (white)
static uint32_t g_bg   = 0x00202840;   // background (dark slate)

void console_init() {
    g_cols = fb_width()   / FONT_W;
    g_rows = fb_height()  / FONT_H;
    g_col  = 0;
    g_row  = 0;
    fb_fill(g_bg);
}

// Paint the glyph for c into character cell (col, row).
static void draw_glyph(uint8_t c, uint32_t col, uint32_t row) {
    const uint8_t* g = font_glyph(c);
    uint32_t px = col * FONT_W;
    uint32_t py = row * FONT_H;
    for (uint32_t y = 0; y < FONT_H; y++) {
        uint8_t bits = g[y];
        for (uint32_t x =0; x < FONT_W; x++) {
            uint32_t color = (bits & (0x80 >> x)) ? g_fg : g_bg;
            fb_putpixel(px + x, py + y, color);
        }
    }
}

static void newline() {
    g_col = 0;
   if (g_row + 1 >= g_rows) {
    fb_scroll_up(FONT_H, g_bg);       // at the bottom: shift up one text line
    // cursor stays on the last row
   } else {
        g_row++;
   }
}

void console_putchar(char c) {
    if (c == '\n') { newline(); return; }
    if (c == '\r') { g_col = 0; return; }
    draw_glyph((uint8_t)c, g_col, g_row);
    if (++g_col >= g_cols) newline();
}

void console_write(const char* s) {
    while (*s) console_putchar(*s++);
}