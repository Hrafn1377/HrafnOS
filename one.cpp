extern "C" {
    static long sys_write(long fd, const char* buf, long len) {
        long r;
        asm volatile("int $0x80" : "=a"(r) : "a"(0L), "D"(fd), "S"(buf), "d"(len) : "memory");
        return r;
    }
    static long sys_fork() {
        long r;
        asm volatile("int $0x80" : "=a"(r) : "a"(5L) : "memory");
        return r;
    }
    static void sys_exit() {
        asm volatile("int $0x80" : : "a"(2L) : "memory");
    }
    void _start() {
        long pid = sys_fork();
        if (pid == 0) {
            const char* c = "[child]";
            for (int k = 0; k < 5; k++) {
                sys_write(1, c, 7);
                for (volatile long i = 0; i < 0x4000000; i++) { }
            }
        } else {
            const char* p = "[parent]";
            for (int k = 0; k < 5; k++) {
                sys_write(1, p, 8);
                for (volatile long i = 0; i < 0x4000000; i++) { }
            }
        }
        sys_exit();
}
}