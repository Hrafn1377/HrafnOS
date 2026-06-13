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

static void free_block(int blk) {
    g_block_used[blk / 8] &= ~(1 << (blk % 8));
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

// Look up a signle component (name, name_len) inside directory `dir`.
// Returns the child inode index, or -1 if not present.
static int dir_lookup(int dir, const char* name, int name_len) {
    Inode* d = &g_inodes[dir];
    for (int b = 0; b < MUNIN_DIRECT; b++) {
        if (d->blocks[b] == NO_BLOCK) continue;
        Dirent* de = (Dirent*)g_blocks[d->blocks[b]];
        for (uint32_t k = 0; k < DIRENTS_PER_BLOCK; k++) {
            if (de[k].inode == -1) continue;
            int i = 0;
            while (i < name_len && de[k].name[i] && de[k].name[i] == name[i]) i++;
            if (i == name_len && de[k].name[i] == '\0') return de[k].inode;
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
    int nl = 0; while (name[nl]) nl++;
    if (nl == 0 || nl >= MUNIN_MAX_NAME)      return -1;
    if (dir_lookup(dir, name, nl) >= 0)       return -1;      // already exists

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

// ==============================================================
// step 1b: path traversal
// ==============================================================

static int str_len(const char* s) { int n = 0; while (s[n]) n++; return n; }

// Resolve the first `len` characters of an absolute path to an inode index.
static int resolve_n(const char* path, int len) {
    if (len < 1 || path[0] != '/') return -1;   // absolute only
    int cur = MUNIN_ROOT;
    int i = 1;                                   // skip leading '/'
    while (i < len) {
        while (i < len && path[i] == '/') i++;   // skip separators
        if (i >= len) break;
        int start = i;
        while (i < len && path[i] != '/') i++;   // span one component
        int clen = i - start;
        if (clen > 0) {
            if (g_inodes[cur].type != INODE_DIR) return -1;  // can't descend a file
            int next = dir_lookup(cur, path + start, clen);
            if (next < 0) return -1;
            cur = next;
        }
    }
    return cur;
}

int munin_resolve(const char* path) {
    if (!path) return -1;
    return resolve_n(path, str_len(path));
}

int munin_create_path(const char* path, uint8_t type) {
    if (!path || path[0] != '/') return -1;
    int len = str_len(path);
    while (len > 1 && path[len - 1] == '/') len--;     // strip trailing '/'

    int slash = 0;
    for (int i = 0; i < len; i++) if (path[i] == '/') slash = i;

    const char* name = path + slash + 1;
    int name_len = len - (slash + 1);
    if (name_len <= 0 || name_len >= MUNIN_MAX_NAME) return -1;

    int parent = (slash == 0) ? MUNIN_ROOT : resolve_n(path, slash);
    if (parent < 0 || g_inodes[parent].type != INODE_DIR) return -1;

    // `name` points into `path` and isn't necessarily NUL-terminated at name_len
    // (a trailing '/' may follow), so copy it into a small buffer first.
    char nbuf[MUNIN_MAX_NAME];
    int i = 0;
    for (; i < name_len; i++) nbuf[i] = name[i];
    nbuf[i] = '\0';

    return munin_create(parent, nbuf, type);
}

int munin_mkdir(const char* path) {
    return munin_create_path(path, INODE_DIR);
}

void munin_ls_path(const char* path) {
    int ino = munin_resolve(path);
    if (ino < 0) { kprint("munin: no such path\n"); return; }
    munin_ls(ino);
}

// ============================================================
// step 1c: file data (read / write)
// ============================================================

static void free_inode_blocks(Inode* f) {
    for (int b = 0; b < MUNIN_DIRECT; b++) {
        if (f->blocks[b] != NO_BLOCK) {
            free_block((int)f->blocks[b]);
            f->blocks[b] = NO_BLOCK;
        }
    }
    f->size = 0;
}

// Overwrite the file at `path` with `len` bytes from `buf`.
// Returns bytes written, or -1 on error (not a file, too big, out of space).
int munin_write(const char* path, const void* buf, uint32_t len) {
    int ino = munin_resolve(path);
    if (ino < 0) return -1;
    Inode* f = &g_inodes[ino];
    if (f->type != INODE_FILE) return -1;

    uint32_t max_bytes = (uint32_t)MUNIN_DIRECT * MUNIN_BLOCK_SIZE;
    if (len > max_bytes) return -1;          // exceeds direct-block capacity

    free_inode_blocks(f);                    // truncate existing content

    const uint8_t* src = (const uint8_t*)buf;
    uint32_t remaining = len;
    int b = 0;
    while (remaining > 0) {
        int blk = alloc_block();
        if (blk < 0) { free_inode_blocks(f); return -1; }   // out of space; roll back
        f->blocks[b] = (uint32_t)blk;
        uint32_t chunk = remaining < MUNIN_BLOCK_SIZE ? remaining : MUNIN_BLOCK_SIZE;
        uint8_t* dst = g_blocks[blk];
        for (uint32_t i = 0; i < chunk; i++) dst[i] = src[i];
        src += chunk;
        remaining -= chunk;
        b++;
    }
    f->size = len;
    return (int)len;
}

// Read up to `len` bytes from the file at `path` into `buf`.
// Returns bytes read, or -1 on error.
int munin_read(const char* path, void* buf, uint32_t len) {
    int ino = munin_resolve(path);
    if (ino < 0) return -1;
    Inode* f = &g_inodes[ino];
    if (f->type != INODE_FILE) return -1;

    uint32_t n = f->size < len ? f->size : len;
    uint8_t* dst = (uint8_t*)buf;
    uint32_t copied = 0;
    int b = 0;
    while (copied < n) {
        if (b >= MUNIN_DIRECT || f->blocks[b] == NO_BLOCK) break;   // safety
        uint32_t chunk = (n - copied) < MUNIN_BLOCK_SIZE ? (n - copied) : MUNIN_BLOCK_SIZE;
        uint8_t* srcb = g_blocks[f->blocks[b]];
        for (uint32_t i = 0; i < chunk; i++) dst[copied + i] = srcb[i];
        copied += chunk;
        b++;
    }
    return (int)copied;
}