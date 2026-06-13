#include "vfs.hpp"
#include "sched.hpp"     // task, file_desc, MAX_FDS, sched_current()
#include "munin.hpp"     // munin_* + INODE_FILE / INODE_DIR
#include "syscall.hpp"   // O_CREAT / O_TRUNC / O_APPEND

int vfs_open(const char* path, int flags) {
    task* t = sched_current();
    if (!t) return -1;

    int ino = munin_resolve(path);
    if (ino < 0) {
        if (flags & O_CREAT) {
            ino = munin_create_path(path, INODE_FILE);
            if (ino < 0) return -1;
        } else {
            return -1;                       // not found and not asked to create
        }
    }

    int ty = munin_type(ino);
    if (ty != INODE_FILE && ty != INODE_DIR) return -1;

    if ((flags & O_TRUNC) && ty == INODE_FILE) munin_truncate(ino);

    uint32_t start = 0;
    if ((flags & O_APPEND) && ty == INODE_FILE) {
        int sz = munin_size(ino);
        if (sz > 0) start = (uint32_t)sz;    // open at end of file
    }

    for (int fd = 3; fd < MAX_FDS; fd++) {   // 0/1/2 reserved for tty
        if (!t->fds[fd].used) {
            t->fds[fd].used   = true;
            t->fds[fd].inode  = ino;
            t->fds[fd].type   = (uint8_t)ty;
            t->fds[fd].offset = start;
            return fd;
        }
    }
    return -1;                               // descriptor table full
}

int vfs_close(int fd) {
    task* t = sched_current();
    if (!t || fd < 3 || fd >= MAX_FDS) return -1;
    if (!t->fds[fd].used) return -1;
    t->fds[fd].used = false;
    return 0;
}

int vfs_read(int fd, void* buf, uint32_t len) {
    task* t = sched_current();
    if (!t || fd < 3 || fd >= MAX_FDS) return -1;
    file_desc* f = &t->fds[fd];
    if (!f->used || f->type != INODE_FILE) return -1;

    int n = munin_read_inode(f->inode, buf, len, f->offset);
    if (n > 0) f->offset += (uint32_t)n;     // advance the cursor
    return n;
}

int vfs_write(int fd, const void* buf, uint32_t len) {
    task* t = sched_current();
    if (!t || fd < 3 || fd >= MAX_FDS) return -1;
    file_desc* f = &t->fds[fd];
    if (!f->used || f->type != INODE_FILE) return -1;

    int n = munin_write_inode(f->inode, buf, len, f->offset);
    if (n > 0) f->offset += (uint32_t)n;     // advance the cursor
    return n;
}