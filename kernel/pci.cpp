#include "pci.hpp"
#include "io.hpp"       // inl / outl
#include "serial.hpp"   // kprint

// ---- config-space access ----
static uint32_t cfg_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    return (1u << 31)
         | ((uint32_t)bus  << 16)
         | ((uint32_t)(slot & 0x1F) << 11)
         | ((uint32_t)(func & 0x07) << 8)
         | (uint32_t)(off & 0xFC);
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    outl(0xCF8, cfg_addr(bus, slot, func, off));
    return inl(0xCFC);
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t val) {
    outl(0xCF8, cfg_addr(bus, slot, func, off));
    outl(0xCFC, val);
}

// ---- tiny hex printer (no dependency on a kernel kprint_hex) ----
static void put_hex(uint32_t v, int digits) {
    static const char* H = "0123456789ABCDEF";
    char buf[9];
    for (int i = 0; i < digits; i++) buf[digits - 1 - i] = H[(v >> (i * 4)) & 0xF];
    buf[digits] = '\0';
    kprint(buf);
}

void pci_scan_dump() {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_read32((uint8_t)bus, slot, func, 0x00);
                uint16_t ven = (uint16_t)(id & 0xFFFF);
                if (ven == 0xFFFF) continue;                 // no device here
                uint16_t dev = (uint16_t)((id >> 16) & 0xFFFF);
                uint32_t cls = pci_read32((uint8_t)bus, slot, func, 0x08);

                kprint("pci ");
                put_hex(bus, 2);  kprint(":"); put_hex(slot, 2); kprint("."); put_hex(func, 1);
                kprint("  ");     put_hex(ven, 4); kprint(":"); put_hex(dev, 4);
                kprint("  class "); put_hex((cls >> 24) & 0xFF, 2);
                kprint(":");        put_hex((cls >> 16) & 0xFF, 2);
                kprint("\n");
            }
        }
    }
}

bool pci_find(uint16_t vendor, uint16_t device,
              uint8_t* bus_out, uint8_t* slot_out, uint8_t* func_out) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_read32((uint8_t)bus, slot, func, 0x00);
                if ((id & 0xFFFF) == 0xFFFF) continue;
                if ((uint16_t)(id & 0xFFFF) == vendor &&
                    (uint16_t)((id >> 16) & 0xFFFF) == device) {
                    *bus_out = (uint8_t)bus; *slot_out = slot; *func_out = func;
                    return true;
                }
            }
        }
    }
    return false;
}