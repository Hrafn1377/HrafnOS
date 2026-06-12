#include "kbd.hpp"

// Set-1 scancode -> ASCII. 0 means "no character" (modifier, release, unmapped).
static const char map_lower[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const char map_upper[128] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static bool g_shift = false;

char kbd_handle_scancode(uint8_t sc) {
    // Key releases have the high bit (0x80) set.
    if (sc & 0x80) {
        uint8_t code = sc & 0x7F;
        if (code == 0x2A || code == 0x36) g_shift = false;   // shift released
        return 0;
    }
    if (sc == 0x2A || sc == 0x36) { g_shift = true; return 0; }

    char c = g_shift ? map_upper[sc] : map_lower[sc];
    if (c == 0x7F) c = '\b';   // some keyboards send DEL for Backspace; normalize
    return c;
}

// --- input ring buffer (signle producer: IRQ1, single consumer: SYS_READ) ---
#define KBD_BUF_SIZE 128
static volatile char            kbd_buf[KBD_BUF_SIZE];
static volatile uint32_t kbd_head = 0;       // next write slot
static volatile uint32_t kbd_tail = 0;       // next read slot

void kbd_buffer_push(char c) {
    uint32_t next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next == kbd_tail) return;        // buffer full: drop the char
    kbd_buf[kbd_head] = c;
    kbd_head = next;
}

int kbd_getchar() {
    if (kbd_tail == kbd_head) return -1; // empty
    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return (int)(unsigned char)c;
}