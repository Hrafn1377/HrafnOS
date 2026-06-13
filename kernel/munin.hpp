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

// ---- path API (step 1b) ----
// Resolve an absolute path ("/docs/notes") to an inode index, or -1.
int  munin_resolve(const char* path);
// Create a file or directort at an absolute path. Returns inode index, or -1.
int  munin_create_path(const char* path, uint8_t type);
// Convenience: create a directory at an absolute path.
int  munin_mkdir(const char* path);
// List the directoy named by an absolute path.
void munin_ls_path(const char* path);

// ---- file data (step 1c) ----
// Overwrite the file at `path` with `len` bytes. Returns bytes written, or -1.
int  munin_write(const char* path, const void* buf, uint32_t len);
// Read up to `len` bytes from the file at `path`. Returns bytes read, or -1.
int  munin_read(const char* path, void* buf, uint32_t len);

// ---- offset-aware primitives (step 2a, for the fd layer) ----
int munin_type(int ino);              // INODE_FREE/FILE/DIR, or -1
int munin_size(int ino);              // file size in bytes, or -1
int munin_truncate(int ino);          // free a file's blocks, size -> 0
int munin_read_inode(int ino, void* buf, uint32_t len, uint32_t offset);
int munin_write_inode(int ino, const void* buf, uint32_t len, uint32_t offset);