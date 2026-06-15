#include "mouse.hpp"
#include "io.hpp"
#include "pic.hpp"
#include "fb.hpp"
#include "serial.hpp"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

static int     g_x, g_y;
static uint8_t g_buttons;
static uint8_t g_packet[3];
static int     g_phase;

static void ps2_wait_in()  { for (int i = 0; i < 100000; i++) if (!(inb(PS2_STATUS) & 0x02)) return; }
static void ps2_wait_out() { for (int i = 0; i < 100000; i++) if  (inb(PS2_STATUS) & 0x01)  return; }

static void ps2_cmd(uint8_t c) { ps2_wait_in(); outb(PS2_CMD, c); }

static void mouse_write(uint8_t b) {
    ps2_wait_in(); outb(PS2_CMD, 0xD4);
    ps2_wait_in(); outb(PS2_DATA, b);
}
static uint8_t mouse_read() { ps2_wait_out(); return inb(PS2_DATA); }

void mouse_init() {
    ps2_cmd(0xA8);                         // enable the aux device

    ps2_cmd(0x20);                         // read config byte
    uint8_t cfg = mouse_read();
    cfg |=  0x02;                          // bit1: aux interrupt on
    cfg &= ~0x20;                          // bit5: aux clock enabled
    ps2_cmd(0x60);                         // write config byte
    ps2_wait_in(); outb(PS2_DATA, cfg);


    mouse_write(0xF6); mouse_read();   // set defaults
    mouse_write(0xF4); mouse_read();  // enable reporting
    

    g_x = (int)fb_width()  / 2;
    g_y = (int)fb_height() / 2;
    g_phase   = 0;
    g_buttons = 0;

    pic_unmask(12);
}

void mouse_irq() {
    uint8_t status = inb(PS2_STATUS);
    if (!(status & 0x01)) return;
    uint8_t data = inb(PS2_DATA);


    switch (g_phase) {
        case 0:
            if (!(data & 0x08)) return;
            g_packet[0] = data; g_phase = 1; break;
        case 1:
            g_packet[1] = data; g_phase = 2; break;
        case 2: {
            g_packet[2] = data; g_phase = 0;
            uint8_t f = g_packet[0];
            if (f & 0xC0) break;
            int dx = (int)g_packet[1];
            int dy = (int)g_packet[2];
            if (f & 0x10) dx -= 256;
            if (f & 0x20) dy -= 256;
            g_x += dx;
            g_y -= dy;
            int w = (int)fb_width(), h = (int)fb_height();
            if (g_x < 0)  g_x = 0;
            if (g_x >= w) g_x = w - 1;
            if (g_y < 0)  g_y = 0;
            if (g_y >= h) g_y = h - 1;
            g_buttons = f & 0x07;
            break;
        }
    }
}

int     mouse_x()       { return g_x; }
int     mouse_y()       { return g_y; }
uint8_t mouse_buttons() { return g_buttons; }