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
    static long sys_exec(char** argv, long argc) {
        long r;
        asm volatile("int $0x80" : "=a"(r) : "a"(4L), "D"(argv), "S"(argc) : "memory");
        return r;
    }
    static void sys_yield() { asm volatile("int $0x80" : : "a"(1L) : "memory"); }
    static void sys_wait()  { asm volatile("int $0x80" : : "a"(6L) : "memory"); }
    static void sys_exit()  { asm volatile("int $0x80" : : "a"(2L) : "memory"); }

    // ---- file syscall wtappers (step 3b) ----
    static long sys_open(const char* path, long flags) {
        long r;
        asm volatile("int $0x80" : "=a"(r) : "a"(7L), "D"(path), "S"(flags): "memory");
        return r;
    }
    static long sys_close(long fd) {
        long r;
        asm volatile("int $0x80" : "=a"(r) : "a"(8L), "D"(fd) : "memory");
        return r;
    }
    static long sys_readdir(long fd, long index, char* name) {
        long r;
        asm volatile("int $0x80" : "=a"(r) : "a"(9L), "D"(fd), "S"(index), "d"(name) : "memory");
        return r;
    }
    static long sys_mkdir(const char* path) {
        long r;
        asm volatile ("int $0x80" : "=a"(r) : "a"(10L), "D"(path) : "memory");
        return r;
    }
    static long sys_unlink(const char* path) {
        long r;
        asm volatile("int $0x80" : "=a"(r) : "a"(11L), "D"(path) : "memory");
        return r;
    }

    static void puts(const char* s) {
        long n = 0;
        while (s[n]) n++;
        sys_write(1, s, n);
    }

    // ---- current working directory + path normalizer (step 3d) ----
    static char cwd[128] = "/";
    
    // Normalize `in` (absolute, or relative to cwd) into a clean absolute path
    // in `out`. Collapses '.', '..', and redundant slashes.
    static void resolve_path(const char* in, char* out) {
        int olen;
        if (in[0] == '/') {
            out[0] = '/'; olen = 1; out[1] = 0;
        } else {
            int i = 0; while (cwd[i]) { out[i] = cwd[i]; i++; } olen = i; out[olen] = 0;
        }
        int p = 0;
        while (in[p]) {
            while (in[p] == '/') p++;            // skip slash run
            if (!in[p]) break;
            int start = p;
            while (in[p] && in[p] != '/') p++;
            int clen = p - start;
            if (clen == 1 && in[start] == '.') {
                // "." -> stay
            } else if (clen == 2 && in[start] == '.' && in[start + 1] == '.') {
                if (olen > 1) {                               // pop last component
                    while (olen > 1 && out[olen - 1] != '/') olen--;
                    if (olen > 1) olen--;                     // drop the slash too
                    out[olen] = 0;
                }
            } else {
                if (out[olen - 1] != '/') out[olen++] = '/';
                for (int k = 0; k < clen; k++) out[olen++] = in[start + k];
                out[olen] = 0;
            }
        }
        if (olen == 0) { out[0] = '/'; out[1] = 0; }
    }

    // ---- tiny helpers + builtins (step 3b) ----
    static bool streq(const char* a, const char* b) {
        int i = 0;
        while (a[i] && a[i] == b[i]) i++;
        return a[i] == b[i];
    }

    static void cmd_ls(int argc, char** argv) {
       const char* arg = (argc >= 2) ? argv[1] : ".";           // no arg -> cwd
        char abspath[128]; resolve_path(arg, abspath);
        long fd = sys_open(abspath, 0);                   // O_RDONLY
        if (fd < 0) { puts("ls: no such path\n"); return; }
        char nm[40];
        for (long i = 0; ; i++) {
            long r = sys_readdir(fd, i, nm);
            if (r <= 0) break;                   // 0 = end, -1 = not a dir
            puts(nm);
            if (r == 2) puts("/");               // 2 = INODE_DIR
            puts("  ");
        }
        puts("\n");
        sys_close(fd);
    }

    static void cmd_cat(int argc, char** argv) {
        if (argc < 2) { puts("cat: missing file\n"); return; }
        char abspath[128]; resolve_path(argv[1], abspath);
        long fd = sys_open(abspath, 0);                    // O_RDONLY
        if (fd < 0) { puts("cat: no such file\n"); return; }
        char buf[128];
        char last = '\n';
        bool any  = false;
        for (;;) {
            long n = sys_read(fd, buf, 128);
            if (n <= 0) break;
            sys_write(1, buf, n);
            last = buf[n - 1];
            any  = true;
        }
        if (any && last != '\n') sys_write(1, "\n", 1);    // fresh line for the prompt
        sys_close(fd);
    }

    static void cmd_mkdir(int argc, char** argv) {
        if (argc < 2) { puts("mkdir: missing path\n"); return; }
        char abspath[128]; resolve_path(argv[1], abspath);
        if (sys_mkdir(abspath) != 0) puts("mkdir: failed\n");
    }

    static void cmd_rm(int argc, char** argv) {
        if (argc < 2) { puts("rm: missing path\n"); return; }
        char abspath[128]; resolve_path(argv[1], abspath);
        if (sys_unlink(abspath) != 0) puts("rm: failed\n");
    }

    // echo [words...]            -> stdout
    // echo [words...] > <file>   -> write to file (needs spaces around '>')
    static void cmd_echo(int argc, char** argv) {
        int redir = -1;
        for (int i = 1; i < argc; i++) if (streq(argv[i], ">")) { redir = i; break; }

        long fd     = 1;        // default: stdout
        int  end    = argc;     // print words argv[1..end)
        bool opened = false;
        if (redir >= 0) {
            if (redir + 1 >= argc) { puts("echo: missing redirect target\n"); return; }
            char abspath[128]; resolve_path(argv[redir + 1], abspath);
            fd = sys_open(abspath, 1 | 4 | 8);    // O_WRONLY|O_CREAT|O_TRUNC
            if (fd < 0) { puts("echo: cannot open target\n"); return; }
            opened = true;
            end = redir;
        }

        for (int i = 1; i < end; i++) {
            long len = 0; while (argv[i][len]) len++;
            sys_write(fd, argv[i], len);
            if (i + 1 < end) sys_write(fd, " ", 1);
        }
        sys_write(fd, "\n", 1);
        if (opened) sys_close(fd);
    }

    static void cmd_cd(int argc, char** argv) {
        const char* arg = (argc >= 2) ? argv[1] : "/";
        char abspath[128]; resolve_path(arg, abspath);
        long fd = sys_open(abspath, 0);
        if (fd < 0) { puts("cd: no such directory\n"); return; }
        char nm[40];
        long r = sys_readdir(fd, 0, nm);       // >= 0 => it is a directory (0 = empty)
        sys_close(fd);
        if (r < 0) { puts("cd: not a directory\n"); return; }
        int i = 0; while (abspath[i]) { cwd[i] = abspath[i]; i++; } cwd[i] = 0;
    }

    void _start() {
        char line[64];
        for (;;) {
           puts(cwd); puts(" $ ");

            // Read a line, char by char, echoing as we go. SYS_READ is non-blocking,
            // so when nothing's typed we yield the CPU instead of spinng.
            int n = 0;
            for (;;) {
                char ch;
                long r = sys_read(0, &ch, 1);
                if (r <= 0) { sys_yield(); continue; }
                if (ch == '\r' || ch == '\n') { sys_write(1, "\n", 1); break; }
                if (ch == '\b') {                  // backspace
                    if (n > 0) { n--; sys_write(1, "\b \b", 3); }     //erase last char
                    continue;                         // (ignored at column 0: guards prompt)
                }
                sys_write(1, &ch, 1);            // echo
                if (n < 63) line[n++] = ch;
            }
            line[n] = 0;
            if (n == 0) continue;          // empty line

            //Split the line into words: argv[0] is the command, the rest are args.
            char* argv[16];
            int   argc = 0;
            int   p = 0;
            while (line[p] && argc < 16) {
                while (line[p] == ' ') p++;         // skip spaces
                if (!line[p]) break;
                argv[argc++] = &line[p];            // start of a word
                while (line[p] && line[p] != ' ') p++;
                if (line[p] == ' ') line[p++] = 0;   // terminates the word
            }
            if (argc == 0) continue;

            // ---- builtins first (step 3b) ----
            if (streq(argv[0], "ls")) { cmd_ls(argc, argv); continue; }
            if (streq(argv[0], "cat")) { cmd_cat(argc, argv); continue; }
            if (streq(argv[0], "mkdir")) { cmd_mkdir(argc, argv); continue; }
            if (streq(argv[0], "rm")) { cmd_rm(argc, argv);      continue; }
            if (streq(argv[0], "echo")) { cmd_echo(argc, argv);  continue; }
            if (streq(argv[0], "cd")) { cmd_cd(argc, argv); continue; }

            long pid = sys_fork();
            if (pid == 0) {
                sys_exec(argv, argc);              // child becomes the command
                puts("?\n");                       // exec returned -> unknown command
                sys_exit();
            }
            sys_wait();                           // parent: block until the command finishes, then re-prompt
        }
}
}