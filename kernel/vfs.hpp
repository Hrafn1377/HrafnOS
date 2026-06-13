#pragma once
#include <stdint.h>

// Minimal VFS layer: maps per-process file descriptors (>= 3) onto munin inodes,
// tracking a stateful read/write offset per fd. Descriptors 0/1/2 (stdin/stdout/
// stderr) are handled as tty directly in the syscall dispatch, not here.

int vfs_open(const char* path, int flags);            // -> fd (>= 3), or -1
int vfs_close(int fd);                                // -> 0, or -1
int vfs_read(int fd, void* buf, uint32_t len);        // -> bytes read, or -1
int vfs_write(int fd, const void* buf, uint32_t len); // -> bytes written, or -1

// Enumerate a directory fd: entry `index` -> name_out (must be >= 32 bytes).
// Returns child type (1=file, 2=dir), 0 at end, -1 on error.
int vfs_readdir(int fd, int index, char* name_out);

int vfs_mkdir(const char* path);       // create directory; 0 ok, -1 err
int vfs_unlink(const char* path);      // remove file/empty dir; 0 ok, -1 err