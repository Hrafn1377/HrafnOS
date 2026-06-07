extern "C" {
static long sys_write(long fd, const char* buf, long len) {
    long r;
    asm volatile("int $0x80" : "=a"(r)
                 : "a"(0L), "D"(fd), "S"(buf), "d"(len) : "memory");
    return r;
}
void _start() {
    const char* msg = "[two]";
    for (;;) {
        sys_write(1, msg, 5);
        for (volatile long i = 0; i < 0x4000000; i++) { }
    }
}
}
