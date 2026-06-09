extern "C" {
static long sys_write(long fd, const char* buf, long len) {
    long r;
    asm volatile("int $0x80" : "=a"(r)
                : "a"(0L), "D"(fd), "S"(buf), "d"(len) : "memory");
    return r;
}
static void sys_exit() { asm volatile("int $0x80" : : "a"(2L) : "memory"); }

void _start() {
    // ANSI: clear screen (2J) then move cursor to home (H).
    const char* esc = "\033[2J\033[H";
    long n = 0;
    while (esc[n]) n++;
    sys_write(1, esc, n);
    sys_exit();
}
}