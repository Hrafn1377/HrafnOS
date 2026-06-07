// A freestanding ring-3 program: no libc, its own _start, syscalls via int 0x80.
// Compiled non-PIE/static and linked at 0x8000000000 (see user.ld), so the
// kernel's ELF loader can place it with no relocation.
extern "C" {

static long sys_write(long fd, const char* buf, long len) {
    long r;
    asm volatile("int $0x80"
                 : "=a"(r)
                 : "a"(0L) /* SYS_WRITE */, "D"(fd), "S"(buf), "d"(len)
                 : "memory");
    return r;
}

void _start() {
    const char* msg = "[elf]";
    for (;;) {
        sys_write(1, msg, 5);
        for (volatile long i = 0; i < 0x4000000; i++) { }   // crude delay
    }
}

}
