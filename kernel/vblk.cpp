#include "vblk.hpp"
#include "pci.hpp"
#include "io.hpp"
#include "serial.hpp"     // kprint, kprint_uint

// ---- legacy virtio-pci register offsets (relative to the I/O BAR base) ----
#define VIRTIO_DEVICE_FEATURES  0x00   // 32-bit RO
#define VIRTIO_GUEST_FEATURES   0x04   // 32-bit RW
#define VIRTIO_QUEUE_PFN        0x08   // 32-bit RW
#define VIRTIO_QUEUE_SIZE       0x0C   // 16-bit RO
#define VIRTIO_QUEUE_SELECT     0x0E   // 16-bit RW
#define VIRTIO_QUEUE_NOTIFY     0x10   // 16-bit RW
#define VIRTIO_STATUS           0x12   //  8-bit RW
#define VIRTIO_ISR              0x13   //  8-bit RO
#define VIRTIO_CONFIG           0x14   // device-specific config (MSI-X disabled)

// ---- device status bits ----
#define VSTAT_ACK         1
#define VSTAT_DRIVER      2
#define VSTAT_DRIVER_OK   4
#define VSTAT_FAILED      128

static uint16_t g_io;            // I/O port base of the device's register block
static uint64_t g_capacity;      // sectors

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

    // Enable I/O space + bus mastering in the PCI command register.
    uint32_t cmd = pci_read32(bus, slot, func, 0x04);
    cmd |= (1u << 0) | (1u << 2);
    pci_write32(bus, slot, func, 0x04, cmd);

    // BAR0 is an I/O-port BAR for the legacy interface; mask off the low bits.
    uint32_t bar0 = pci_read32(bus, slot, func, 0x10);
    g_io = (uint16_t)(bar0 & 0xFFFC);

    // Handshake: reset, then ACKNOWLEDGE + DRIVER.
    outb(g_io + VIRTIO_STATUS, 0);
    outb(g_io + VIRTIO_STATUS, VSTAT_ACK);
    outb(g_io + VIRTIO_STATUS, VSTAT_ACK | VSTAT_DRIVER);

    // Read the device's offered features (informational); accept none for now.
    uint32_t feats = inl(g_io + VIRTIO_DEVICE_FEATURES);
    outl(g_io + VIRTIO_GUEST_FEATURES, 0);

    // Capacity is a 64-bit field (in sectors) at the start of device config.
    uint32_t cap_lo = inl(g_io + VIRTIO_CONFIG + 0);
    uint32_t cap_hi = inl(g_io + VIRTIO_CONFIG + 4);
    g_capacity = ((uint64_t)cap_hi << 32) | cap_lo;

    kprint("virtio-blk: io=0x"); put_hex(g_io, 4);
    kprint(" features=0x");      put_hex(feats, 8);
    kprint(" capacity=");        kprint_uint((uint32_t)g_capacity);
    kprint(" sectors\n");
    return true;
}

uint64_t vblk_capacity() { return g_capacity; }