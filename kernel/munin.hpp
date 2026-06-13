#pragma once
#include <stdint.h>

// ---- munin: an in-RAM hierarchical filesystem (step 1) ----
// Fixed preallocated region living in the kernel's BSS.

#define MUNIN_MAX_INODES 128
#define MUNIN_NUM_BLOCKS 512
#define MUNIN_BLOCK_SIZE 512    // 512 * 512 = 256 KiB of data region
#define MUNIN_MAX_NAME   32
#define MUNIN_DIRECT     8      // direct block pointers per inode

#define MUNIN_ROOT       0      // root directory is always inode 0
#define NO_BLOCK         0xFFFFFFFFu

enum InodeType { INODE_FREE = 0, INODE_FILE = 1, INODE_DIR = 2 };

// Format the region and create the (empty) root directory.
void munin_init();

// Create a file or directory named `name` inside directory inode `dir`.
// Returns the new inode index, or -1 on failure.
int munin_create(int dir, const char* name, uint8_t type);

// Print the entries of directory inode `dir`.
void munin_ls(int dir);