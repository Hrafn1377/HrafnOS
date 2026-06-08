extern "C" {
static long sys_write(long fd, const char* buf, long len) {
    long r;
    asm volatile("int $0x80" : "=a"(r)
                 : "a"(0L), "D"(fd), "S"(buf), "d"(len) : "memory");
    return r;
}
static long sys_exec(const char* name) {
    long r;
    asm volatile("int $0x80" : "=a"(r) : "a"(4L), "D"(name) : "memory");
    return r;   // returns only on failure
}
void _start() {
    const char* msg = "[two]";
    for (int k = 0; k < 3; k++) {
        sys_write(1, msg, 5);
        for (volatile long i = 0; i < 0x4000000; i++) { }
    }
    sys_exec("one");                       // ping-pong back to "one"
    const char* err = "[exec failed]";
    for (;;) {
        sys_write(1, err, 13);
        for (volatile long i = 0; i < 0x4000000; i++) { }
    }
}
}
