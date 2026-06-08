#pragma once
#include <stdint.h>

// Syscall numbers (rax). Args in rdi, rsi, rdx (SysV/Linux-style).
#define SYS_WRITE 0     // write(fd, buf, len)  -> bytes written
#define SYS_YIELD 1     // yield()
#define SYS_EXIT  2     // exit()               -> never returns
#define SYS_READ  3     // read(fd, buf, len)   -> bytes read (non-blocking)
#define SYS_EXEC  4     // exec(name)           -> -1 on failure, else never returns
