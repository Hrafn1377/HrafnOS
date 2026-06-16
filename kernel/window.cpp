#include "window.hpp"

#define WIN_MAX          16
#define TITLEBAR_H       24
#define DESKTOP_COLOR    RGB(30, 30, 60)
#define TITLE_INACTIVE   RGB(60, 90, 160)
#define TITLE_ACTIVE     RGB(80, 130, 220)
#define BORDER_COLOR     RGB(200, 200, 210)

struct Window { int x, y, w, h; uint32_t content; };
static Window g_win[WIN_MAX];
static int    g_count = 0;

static int  g_drag = -1, g_off_x = 0, g_off_y = 0;
static bool g_last_left = false;

void window_create(int x, int y, int w, int h, uint32_t content_color) {
    if (g_count >= WIN_MAX) return;
    g_win[g_count] = Window{ x, y, w, h, content_color };
    g_count++;
}

void window_compose(Surface* s) {
    gfx_clear(s, DESKTOP_COLOR);
    for (int i = 0; i < g_count; i++) {            // bottom -> top
        Window* w = &g_win[i];
        bool active = (i == g_count - 1);
        gfx_fill_rect(s, w->x, w->y, w->w, TITLEBAR_H, active ? TITLE_ACTIVE : TITLE_INACTIVE);
        gfx_fill_rect(s, w->x, w->y + TITLEBAR_H, w->w, w->h - TITLEBAR_H, w->content);
        gfx_rect(s, w->x, w->y, w->w, w->h, BORDER_COLOR);
    }
}

static int topmost_at(int mx, int my) {
    for (int i = g_count - 1; i >= 0; i--) {       // top -> bottom
        Window* w = &g_win[i];
        if (mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h) return i;
    }
    return -1;
}

static void raise(int i) {
    if (i < 0 || i >= g_count - 1) return;         // already top / invalid
    Window tmp = g_win[i];
    for (int k = i; k < g_count - 1; k++) g_win[k] = g_win[k + 1];
    g_win[g_count - 1] = tmp;
}

void window_handle(int mx, int my, bool left) {
    if (left && !g_last_left) {                    // button press edge
        int hit = topmost_at(mx, my);
        if (hit >= 0) {
            raise(hit);                            // bring to front
            Window* w = &g_win[g_count - 1];
            if (my >= w->y && my < w->y + TITLEBAR_H) {   // pressed on title bar -> drag
                g_drag  = g_count - 1;
                g_off_x = mx - w->x;
                g_off_y = my - w->y;
            }
        }
    } else if (!left && g_last_left) {             // button release edge
        g_drag = -1;
    }
    if (g_drag >= 0 && left) {                     // dragging
        Window* w = &g_win[g_drag];
        w->x = mx - g_off_x;
        w->y = my - g_off_y;
    }
    g_last_left = left;
}