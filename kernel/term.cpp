#include "term.hpp"
#include "gfx.hpp"
#include "font.hpp"

#define TERM_COLS 64
#define TERM_ROWS 20
#define TERM_FG   RGB(210, 220, 210)
#define TERM_BG   RGB(15, 15, 25)

static char g_grid[TERM_ROWS][TERM_COLS];
static int  g_col = 0, g_row = 0;

void term_init() {
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_COLS; c++) g_grid[r][c] = ' ';
    g_col = 0; g_row = 0;
}

static void term_clear() {
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_COLS; c++) g_grid[r][c] = ' ';
    g_col = 0; g_row = 0;
}

static void term_newline() {
    g_col = 0;
    if (g_row + 1 >= TERM_ROWS) {
        for (int r = 0; r < TERM_ROWS - 1; r++)
            for (int c = 0; c < TERM_COLS; c++) g_grid[r][c] = g_grid[r + 1][c];
        for (int c = 0; c < TERM_COLS; c++) g_grid[TERM_ROWS - 1][c] = ' ';
    } else {
        g_row++;
    }
}

enum TState { ST_NORMAL, ST_ESC, ST_CSI };
static TState   g_state = ST_NORMAL;
static uint32_t g_param = 0;
static bool     g_have_param = false;

void term_putchar(char c) {
    if (g_state == ST_ESC) {
        if (c == '[') { g_state = ST_CSI; g_param = 0; g_have_param = false; }
        else            g_state = ST_NORMAL;
        return;
    }
    if (g_state == ST_CSI) {
        if (c >= '0' && c <= '9') { g_param = g_param * 10 + (uint32_t)(c - '0'); g_have_param = true; return; }
        if (c == 'J') { if (!g_have_param || g_param == 2) term_clear(); }
        else if (c == 'H') { g_col = 0; g_row = 0; }
        g_state = ST_NORMAL;
        return;
    }
    if (c == '\033') { g_state = ST_ESC; return; }

    if (c == '\n') { term_newline(); return; }
    if (c == '\r') { g_col = 0; return; }
    if (c == '\b') { if (g_col > 0) { g_col--; g_grid[g_row][g_col] = ' '; } return; }
    if (c == '\t') {
        int n = (g_col + 8) & ~7;
        while (g_col < n && g_col < TERM_COLS) g_grid[g_row][g_col++] = ' ';
        if (g_col >= TERM_COLS) term_newline();
        return;
    }
    if ((uint8_t)c < 0x20 || (uint8_t)c > 0x7E) return;
    g_grid[g_row][g_col] = c;
    if (++g_col >= TERM_COLS) term_newline();
}

void term_render(Surface* s, int x, int y) {
    gfx_fill_rect(s, x, y, TERM_COLS * (int)FONT_W, TERM_ROWS * (int)FONT_H, TERM_BG);
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_COLS; c++) {
            char ch = g_grid[r][c];
            if (ch != ' ') gfx_char(s, x + c * (int)FONT_W, y + r * (int)FONT_H, ch, TERM_FG);
        }
    gfx_fill_rect(s, x + g_col * (int)FONT_W, y + g_row * (int)FONT_H + (int)FONT_H - 2,
                  (int)FONT_W, 2, TERM_FG);   // cursor underline
}