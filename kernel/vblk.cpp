#include "vblk.hpp"
#include "pci.hpp"
#include "io.hpp"
#include "serial.hpp"   // kprint, kprint_uint

// ---- legacy virtio-pci register offsets (relative to the I/O BAR base) ----
#define VIRTIO_DEVICE_FEATURES  0x00
#define VIRTIO_GUEST_FEATURES   0x04
#define VIRTIO_QUEUE_PFN        0x08
#define VIRTIO_QUEUE_SIZE       0x0C
#define VIRTIO_QUEUE_SELECT     0x0E
#define VIRTIO_QUEUE_NOTIFY     0x10
#define VIRTIO_STATUS           0x12
#define VIRTIO_ISR              0x13
#define VIRTIO_CONFIG           0x14

#define VSTAT_ACK        1
#define VSTAT_DRIVER     2
#define VSTAT_DRIVER_OK  4
#define VSTAT_FAILED     128

#define VIRTQ_DESC_F_NEXT   1
#define VIRTQ_DESC_F_WRITE  2

#define VIRTIO_BLK_T_IN     0   // read (device -> memory)
#define VIRTIO_BLK_T_OUT    1   // write (memory -> device)

struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
};

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

// The kernel is identity-mapped (virtual == physical), so a pointer to BSS
// is also its physical address — which is what the device needs.
static inline uint64_t phys(const void* p) { return (uint64_t)(uintptr_t)p; }
static inline uint32_t align_up(uint32_t x, uint32_t a) { return (x + a - 1) & ~(a - 1); }

// ---- the virtqueue (one contiguous, page-aligned region in BSS) ----
alignas(4096) static uint8_t g_queue[16384];   // big enough for queue_size <= 256
static uint16_t      g_io;
static uint64_t      g_capacity;
static uint16_t      g_qsize;
static virtq_desc*   g_desc;
static uint8_t*      g_avail;       // flags@0, idx@2, ring@4
static uint8_t*      g_used;        // flags@0, idx@2, ring@4
static uint16_t      g_avail_idx;   // shadow of avail->idx

// ---- request scratch (all in BSS, so physically addressable) ----
static virtio_blk_req g_hdr;
static uint8_t        g_status;
alignas(512) static uint8_t g_data[512];   // bounce buffer

static void put_hex(uint32_t v, int digits) {
    static const char* H = "0123456789ABCDEF";
    char buf[9];
    for (int i = 0; i < digits; i++) buf[digits - 1 - i] = H[(v >> (i * 4)) & 0xF];
    buf[digits] = '\0';
    kprint(buf);
}

bool vblk_init() {
    uint8_t bus, slot, func;
    if (!pci_find(0x1AF4, 0x1001, &bus, &slot, &func)) {
        kprint("virtio-blk: not found\n");
        return false;
    }

    uint32_t cmd = pci_read32(bus, slot, func, 0x04);
    cmd |= (1u << 0) | (1u << 2);                  // I/O space + bus master
    pci_write32(bus, slot, func, 0x04, cmd);

    uint32_t bar0 = pci_read32(bus, slot, func, 0x10);
    g_io = (uint16_t)(bar0 & 0xFFFC);

    outb(g_io + VIRTIO_STATUS, 0);
    outb(g_io + VIRTIO_STATUS, VSTAT_ACK);
    outb(g_io + VIRTIO_STATUS, VSTAT_ACK | VSTAT_DRIVER);

    uint32_t feats = inl(g_io + VIRTIO_DEVICE_FEATURES);
    (void)feats;
    outl(g_io + VIRTIO_GUEST_FEATURES, 0);

    uint32_t cap_lo = inl(g_io + VIRTIO_CONFIG + 0);
    uint32_t cap_hi = inl(g_io + VIRTIO_CONFIG + 4);
    g_capacity = ((uint64_t)cap_hi << 32) | cap_lo;

    // ---- set up virtqueue 0 ----
    outw(g_io + VIRTIO_QUEUE_SELECT, 0);
    g_qsize = inw(g_io + VIRTIO_QUEUE_SIZE);
    if (g_qsize == 0 || g_qsize > 256) {
        kprint("virtio-blk: bad queue size\n");
        return false;
    }

    for (uint32_t i = 0; i < sizeof(g_queue); i++) g_queue[i] = 0;

    uint32_t desc_bytes  = 16u * g_qsize;
    uint32_t avail_bytes = 6u + 2u * g_qsize;
    uint32_t used_off    = align_up(desc_bytes + avail_bytes, 4096);

    g_desc      = (virtq_desc*)g_queue;
    g_avail     = g_queue + desc_bytes;
    g_used      = g_queue + used_off;
    g_avail_idx = 0;

    uint64_t pfn = phys(g_queue) >> 12;
    outl(g_io + VIRTIO_QUEUE_PFN, (uint32_t)pfn);

    // queue is live; tell the device we're good to go
    outb(g_io + VIRTIO_STATUS, VSTAT_ACK | VSTAT_DRIVER | VSTAT_DRIVER_OK);

    kprint("virtio-blk: io=0x"); put_hex(g_io, 4);
    kprint(" qsize=");           kprint_uint(g_qsize);
    kprint(" capacity=");        kprint_uint((uint32_t)g_capacity);
    kprint(" sectors\n");
    return true;
}

uint64_t vblk_capacity() { return g_capacity; }

static bool vblk_request(uint64_t sector, void* buf, bool write) {
    g_hdr.type     = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    g_hdr.reserved = 0;
    g_hdr.sector   = sector;
    g_status       = 0xFF;

    if (write) {
        uint8_t* s = (uint8_t*)buf;
        for (int i = 0; i < 512; i++) g_data[i] = s[i];
    }

    // 3-descriptor chain: header -> data -> status
    g_desc[0].addr  = phys(&g_hdr);
    g_desc[0].len   = sizeof(virtio_blk_req);
    g_desc[0].flags = VIRTQ_DESC_F_NEXT;
    g_desc[0].next  = 1;

    g_desc[1].addr  = phys(g_data);
    g_desc[1].len   = 512;
    g_desc[1].flags = VIRTQ_DESC_F_NEXT | (write ? 0 : VIRTQ_DESC_F_WRITE);
    g_desc[1].next  = 2;

    g_desc[2].addr  = phys(&g_status);
    g_desc[2].len   = 1;
    g_desc[2].flags = VIRTQ_DESC_F_WRITE;
    g_desc[2].next  = 0;

    // publish head descriptor 0 into the available ring
    volatile uint16_t* avail_idx = (volatile uint16_t*)(g_avail + 2);
    uint16_t*          avail_ring = (uint16_t*)(g_avail + 4);
    avail_ring[g_avail_idx % g_qsize] = 0;
    asm volatile("" ::: "memory");
    g_avail_idx++;
    *avail_idx = g_avail_idx;
    asm volatile("" ::: "memory");

    // kick the device and poll the used ring for completion
    volatile uint16_t* used_idx = (volatile uint16_t*)(g_used + 2);
    uint16_t before = *used_idx;
    outw(g_io + VIRTIO_QUEUE_NOTIFY, 0);
    while (*used_idx == before) asm volatile("pause");

    if (!write) {
        uint8_t* d = (uint8_t*)buf;
        for (int i = 0; i < 512; i++) d[i] = g_data[i];
    }
    return g_status == 0;   // VIRTIO_BLK_S_OK
}

bool vblk_read(uint64_t sector, void* buf)  { return vblk_request(sector, buf, false); }
bool vblk_write(uint64_t sector, void* buf) { return vblk_request(sector, buf, true); }