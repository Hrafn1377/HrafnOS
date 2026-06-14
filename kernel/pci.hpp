#pragma once
#include <stdint.h>

// Read/write a 32-bit dword from PCI configuration space (port 0xCF8/0xCFC).
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
void     pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t val);

// Enumerate the whole bus and print every device found (P1a diagnostic).
void pci_scan_dump();

// Find the first device matching vendor:device. Returns true + its location.
bool pci_find(uint16_t vendor, uint16_t device,
              uint8_t* bus_out, uint8_t* slot_out, uint8_t* func_out);