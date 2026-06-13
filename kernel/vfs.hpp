#pragma once
#include <stdint.h>

// Minimal VFS layer: maps per-process file descriptors (>= 3) onto munin inodes,
// tracking a stateful read/write offset per fd. Descriptors 0/1/2 (stdin/stdout/
// stderr) are handled as tty directly in the syscall dispatch, not here.

int vfs_open(const char* path, int flags);            // -> fd (>= 3), or -1
int vfs_close(int fd);                                // -> 0, or -1
int vfs_read(int fd, void* buf, uint32_t len);        // -> bytes read, or -1
int vfs_write(int fd, const void* buf, uint32_t len); // -> bytes written, or -1