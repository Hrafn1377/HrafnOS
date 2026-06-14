#pragma once
#include <stdint.h>

// Find the virtio-blk device, run the legacy init handshake, read capacity.
// Return true if the device is present and responded.
bool        vblk_init();
uint64_t vblk_capacity();        // disk size in 512-byte sectors (valid after init)
bool     vblk_read(uint64_t sector, void* buf);  // reads 512 bytes; true on success
bool     vblk_write(uint64_t sector, void* buf);  // write 512 bytes; true on success