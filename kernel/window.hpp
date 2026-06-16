#pragma once
#include <stdint.h>
#include "gfx.hpp"

void window_create(int x, int y, int w, int h, uint32_t content_color, const char* title, bool is_term = false);
void window_compose(Surface* s);            // draw desktop + all windows (z-order)
void window_handle(int mx, int my, bool left);  // mouse: raise on click, drag by title bar