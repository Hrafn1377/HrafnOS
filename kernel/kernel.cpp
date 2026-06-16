#include "serial.hpp"
#include "gdt.hpp"
#include "idt.hpp"
#include "pic.hpp"
#include "pmm.hpp"
#include "vmm.hpp"
#include "heap.hpp"
#include "sched.hpp"
#include "elf.hpp"
#include "ramdisk.hpp"
#include "userspace.hpp"
#include "fb.hpp"
#include "console.hpp"
#include "munin.hpp"
#include "vfs.hpp"
#include "syscall.hpp"
#include "pci.hpp"
#include "vblk.hpp"
#include "gfx.hpp"
#include "mouse.hpp"
#include "window.hpp"

// Look a program up in the ramdisk, load it into a fresh address space, give it
// a user stack, and queue it as a ring-3 task.
static void spawn(const char* name) {
    uint8_t* elf = ramdisk_lookup(name);
    if (!elf) {
        kprint("spawn: not found: ");
        kprint(name);
        kprint_char('\n');
        return;
    }

    uint64_t  entry = 0;
    uint64_t* space = elf_load(elf, &entry);
    if (!space) {
        kprint("spawn: load failed: ");
        kprint(name);
        kprint_char('\n');
        return;
    }

    for (uint64_t off = 0; off < USER_STACK_SIZE; off += FRAME_SIZE) {
        uint64_t f = (uint64_t)pmm_alloc_frame();
        vmm_map_page_in(space, (USER_STACK_TOP - USER_STACK_SIZE) + off, f,
                        PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    task_create_user(space, entry, USER_STACK_TOP);
}

extern "C" void kmain(uint64_t mb_info) {
    serial_init();
    kprint("HrafnOS booting...\n");

    gdt_init();
    idt_init();
    pic_remap();
    pit_init(100);

    pmm_init(mb_info);
    uint64_t total_mib = (pmm_total_frame_count() * FRAME_SIZE) / (1024 * 1024);
    kprint("Physical memory: ");
    kprint_uint((uint32_t)total_mib);
    kprint(" MiB, ");
    kprint_uint((uint32_t)pmm_free_frame_count());
    kprint(" frames free\n");

    vmm_init();
    heap_init();
    kprint("Heap ready.\n");
    // Bring up the framebuffer and draw a test pattern to prove we own the screen
    if (fb_init(mb_info)) {
        console_init();
        vblk_init();
        if (munin_mount()) {
            kprint("munin: mounted existing filesystem\n");
        } else {
            munin_init();
            munin_mkdir("/docs");
            munin_mkdir("/bin");
            munin_create_path("/docs/readme", INODE_FILE);
            munin_write("/docs/readme", "Welcome to HrafnOS.\n", 20);
            munin_flush();
            kprint("munin: formatted new filesystem\n");
        }
        kprint("HrafnOS console ready.\n");
    }

    kprint("ramdisk: ");
    for (uint32_t i = 0; i < ramdisk_count(); i++) {
        const ramdisk_entry* e = ramdisk_get(i);
        kprint(e->name);
        kprint(" (");
        kprint_uint((uint32_t)(e->end - e->start));
        kprint(" bytes)");
        if (i + 1 < ramdisk_count()) kprint(", ");
    }
    kprint_char('\n');

    mouse_init();
    sched_init();
    {
        window_create(120, 100, 280, 200, RGB(235, 235, 240));
        window_create(360, 220, 300, 180, RGB(220, 230, 245));
        window_create(200, 320, 260, 160, RGB(245, 235, 220));

        Surface bb = gfx_backbuffer();
        asm volatile("sti");
        for (;;) {
            int  mx   = mouse_x();
            int  my   = mouse_y();
            bool left = mouse_buttons() & 1;

            window_handle(mx, my, left);   // raise / drag
            window_compose(&bb);           // desktop + windows
            gfx_cursor(&bb, mx, my);       // pointer on top
            gfx_present();

            for (volatile int d = 0; d < 300000; d++) { }
        }
    }
    kprint("HrafnOS shell. Type 'help' for a list of commands.\n");

    asm volatile("sti");
    while (true) asm volatile("hlt");
}
