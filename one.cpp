extern "C" {
static long sys_write(long fd, const char* buf, long len) {
    long r;
    asm volatile("int $0x80" : "=a"(r)
                 : "a"(0L), "D"(fd), "S"(buf), "d"(len) : "memory");
    return r;
}
static void sys_exit() { asm volatile("int $0x80" : : "a"(2L) : "memory"); }

void _start() {
    const char* msg = "hello from one\n";
    long n = 0;
    while (msg[n]) n++;
    sys_write(1, msg, n);
    sys_exit();
}
}