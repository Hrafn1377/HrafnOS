extern "C" {
    static long sys_write(long fd, const char* buf, long len) {
        long r;
        asm volatile("int $0x80" : "=a"(r)
                    : "a"(0L), "D"(fd), "S"(buf), "d"(len) : "memory");
        return r;
    }
    static long sys_read(long fd, char* buf, long len) {
        long r;
        asm volatile("int $0x80" : "=a"(r)
                    : "a"(3L), "D"(fd), "S"(buf), "d"(len) : "memory");
        return r;
    }
    static long sys_fork() {
        long r;
        asm volatile("int $0x80" : "=a"(r) : "a"(5L) : "memory");
        return r;
    }
    static long sys_exec(const char* name) {
        long r;
        asm volatile("int $0x80" : "=a"(r) : "a"(4L), "D"(name) : "memory");
        return r;
    }
    static void sys_yield() { asm volatile("int $0x80" : : "a"(1L) : "memory"); }
    static void sys_wait()  { asm volatile("int $0x80" : : "a"(6L) : "memory"); }
    static void sys_exit()  { asm volatile("int $0x80" : : "a"(2L) : "memory"); }

    static void puts(const char* s) {
        long n = 0;
        while (s[n]) n++;
        sys_write(1, s, n);
    }

    void _start() {
        char line[64];
        for (;;) {
            puts("$ ");

            // Read a line, char by char, echoing as we go. SYS_READ is non-blocking,
            // so when nothing's typed we yield the CPU instead of spinng.
            int n = 0;
            for (;;) {
                char ch;
                long r = sys_read(0, &ch, 1);
                if (r <= 0) { sys_yield(); continue; }
                if (ch == '\r' || ch == '\n') { sys_write(1, "\n", 1); break; }
                sys_write(1, &ch, 1);            // echo
                if (n < 63) line[n++] = ch;
            }
            line[n] = 0;
            if (n == 0) continue;        // empty line

            long pid = sys_fork();
            if (pid == 0) {
                sys_exec(line);          // child becomes the command
                puts("?\n");             // exec returned -> unknown command
                sys_exit();
            }
            sys_wait();           // parent: block until the command finished, then re-prompt
        }
}
}