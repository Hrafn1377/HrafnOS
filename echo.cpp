extern "C" {
static long sys_write(long fd, const char* buf, long len) {
    long r;
    asm volatile("int $0x80" : "=a"(r)
                : "a"(0L), "D"(fd), "S"(buf), "d"(len) : "memory");
    return r;
}
static void sys_exit() { asm volatile("int $0x80" : : "a"(2L) : "memory"); }

static void puts(const char* s) {
    long n = 0;
    while (s[n]) n++;
    sys_write(1, s, n);
}

// echo: print argv[1..] separated by spaces, then a newline.
void _start(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        puts(argv[i]);
        if (i + 1 < argc) sys_write(1, " ", 1);
    }
    sys_write(1, "\n", 1);
    sys_exit();
}
}