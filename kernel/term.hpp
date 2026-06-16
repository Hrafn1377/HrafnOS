#pragma once
#include <stdint.h>
#include "gfx.hpp"

void term_init();          
void term_putchar(char c);             // shell output sink (handles \n \r \b \t + ANSI clear/home)
void term_render(Surface* s, int x, int y);  // draw the grid at pixel (x,y)