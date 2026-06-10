#include "fb.hpp"
#include "vmm.hpp"
#include "serial.hpp"

struct mb_tag { uint32_t type; uint32_t size; };

// Multiboot2 framebuffer info tag (type 8).
struct mb_fb_tag {
    uint32_t type;
    uint32_t size;
    uint64_t addr;      // physical address of the framebuffer
    uint32_t pitch;     // bytes per scanline
    uint32_t width;     // pixels
    uint32_t height;    // pixels
    uint8_t  bpp;       // bits per pixel
    uint8_t  fb_type;   // 1 = direct RGB
    uint8_t  reserved;
    // color_info follows
} __attribute__((packed));

static volatile uint8_t* g_fb       = nullptr;
static uint32_t          g_pitch    = 0;
static uint32_t          g_width    = 0;
static uint32_t          g_height   = 0;
static uint32_t          g_bpp      = 0;

bool fb_ready()  { return g_fb != nullptr; }
uint32_t fb_width()  { return g_width; }
uint32_t fb_height() { return g_height; }

bool fb_init(uint64_t mb_info_addr) {
    mb_tag*      tag = (mb_tag*)(mb_info_addr + 8);     // skip total_size + reserved
    mb_fb_tag* fb = nullptr;
    while (tag->type != 0) {
        if (tag->type == 8) { fb = (mb_fb_tag*)tag; break; }
        tag = (mb_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7u));
    }
    if (!fb) {
        kprint("fb: no framebuffer tag from GRUB\n");
        return false;
    }

    g_pitch   = fb->pitch;
    g_width   = fb->width;
    g_height  = fb->height;
    g_bpp     = fb->bpp;

    kprint("fb: addr=");   kprint_ptr((void*)fb->addr);
    kprint(" ");           kprint_uint(g_width);
    kprint("x");           kprint_uint(g_height);
    kprint(" bpp=");       kprint_uint(g_bpp);
    kprint(" pitch=");     kprint_uint(g_pitch);
    kprint(" type=");      kprint_uint(fb->fb_type);
    kprint_char('\n');

    if (g_bpp != 32) {
        kprint("fb: expected 32 bpp; bailing for now\n");
        return false;
    }

    // Identity-map the framebuffer (virt = phys) into kernel space.
    uint64_t base = fb->addr & ~0xFFFULL;
    uint64_t bytes = (uint64_t)g_pitch * g_height;
    uint64_t pages = (bytes + 0xFFF) / 0x1000;
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t a = base + i * 0x1000;
        vmm_map_page(a, a, PAGE_PRESENT | PAGE_WRITABLE);
    }

    g_fb = (volatile uint8_t*)fb->addr;
    kprint("fb: mapped ");
    kprint_uint((uint32_t)pages);
    kprint(" pages\n");
    return true;
}

void fb_fill(uint32_t color) {
    if (!g_fb) return;
    for (uint32_t y = 0; y < g_height; y++) {
        volatile uint32_t* row = (volatile uint32_t*)(g_fb + (uint64_t)y * g_pitch);
        for (uint32_t x = 0; x < g_width; x++) row[x] = color;
    }
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!g_fb || x>= g_width || y >= g_height) return;
    *(volatile uint32_t*)(g_fb + (uint64_t)y * g_pitch + (uint64_t)x * 4) = color;
}