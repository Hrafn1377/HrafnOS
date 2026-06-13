#include "munin.hpp"
#include "serial.hpp"   // kprint

// On-disk-style structures, but for now they live entirely in RAM.

struct Inode {
    uint8_t  type;                 // InodeType
    uint32_t size;                 // file: bytes used; dir: number of entries
    uint32_t blocks[MUNIN_DIRECT]; // direct block indices, or NO_BLOCK
};

struct Dirent {
    int32_t inode;                 // child inode index, -1 = empty slot
    char    name[MUNIN_MAX_NAME];
};

// ---- the fixed preallocated region (BSS) ----
static Inode   g_inodes[MUNIN_MAX_INODES];
static uint8_t g_blocks[MUNIN_NUM_BLOCKS][MUNIN_BLOCK_SIZE];
static uint8_t g_block_used[MUNIN_NUM_BLOCKS / 8];

static const uint32_t DIRENTS_PER_BLOCK = MUNIN_BLOCK_SIZE / sizeof(Dirent);

// ---- block + inode allocators ----
static int alloc_block() {
    for (int i = 0; i < MUNIN_NUM_BLOCKS; i++) {
        if (!(g_block_used[i / 8] & (1 << (i % 8)))) {
            g_block_used[i / 8] |= (1 << (i % 8));
            return i;
        }
    }
    return -1;
}

static int alloc_inode(uint8_t type) {
    for (int i = 0; i < MUNIN_MAX_INODES; i++) {
        if (g_inodes[i].type == INODE_FREE) {
            g_inodes[i].type = type;
            g_inodes[i].size = 0;
            for (int b = 0; b < MUNIN_DIRECT; b++)
                g_inodes[i].blocks[b] = NO_BLOCK;
            return i;
        }
    }
    return -1;
}

// Add a (name -> child) entry into directory `dir`.
static bool add_dirent(int dir, const char* name, int child) {
    Inode* d = &g_inodes[dir];

    for (int b = 0; b < MUNIN_DIRECT; b++) {
        // Allocate a fresh block for this slot if the dir hasn't grown into it yet.
        if (d->blocks[b] == NO_BLOCK) {
            int blk = alloc_block();
            if (blk < 0) return false;
            d->blocks[b] = (uint32_t)blk;
            Dirent* de = (Dirent*)g_blocks[blk];
            for (uint32_t k = 0; k < DIRENTS_PER_BLOCK; k++)
                de[k].inode = -1;        // mark every slot empty
        }

        Dirent* de = (Dirent*)g_blocks[d->blocks[b]];
        for (uint32_t k = 0; k < DIRENTS_PER_BLOCK; k++) {
            if (de[k].inode == -1) {
                de[k].inode = child;
                int i = 0;
                for (; i < MUNIN_MAX_NAME - 1 && name[i]; i++)
                    de[k].name[i] = name[i];
                de[k].name[i] = '\0';
                d->size++;
                return true;
            }
        }
    }
    return false;   // directory full
}

void munin_init() {
    for (int i = 0; i < MUNIN_MAX_INODES; i++) g_inodes[i].type = INODE_FREE;
    for (int i = 0; i < MUNIN_NUM_BLOCKS / 8; i++) g_block_used[i] = 0;

    // Root directory must be inode 0.
    alloc_inode(INODE_DIR);
}

int munin_create(int dir, const char* name, uint8_t type) {
    if (dir < 0 || dir >= MUNIN_MAX_INODES) return -1;
    if (g_inodes[dir].type != INODE_DIR)    return -1;

    int ino = alloc_inode(type);
    if (ino < 0) return -1;

    if (!add_dirent(dir, name, ino)) {
        g_inodes[ino].type = INODE_FREE;   // roll back
        return -1;
    }
    return ino;
}

void munin_ls(int dir) {
    if (dir < 0 || dir >= MUNIN_MAX_INODES || g_inodes[dir].type != INODE_DIR) {
        kprint("munin_ls: not a directory\n");
        return;
    }
    Inode* d = &g_inodes[dir];
    for (int b = 0; b < MUNIN_DIRECT; b++) {
        if (d->blocks[b] == NO_BLOCK) continue;
        Dirent* de = (Dirent*)g_blocks[d->blocks[b]];
        for (uint32_t k = 0; k < DIRENTS_PER_BLOCK; k++) {
            if (de[k].inode != -1) {
                kprint(de[k].name);
                if (g_inodes[de[k].inode].type == INODE_DIR) kprint("/");
                kprint("  ");
            }
        }
    }
    kprint("\n");
}