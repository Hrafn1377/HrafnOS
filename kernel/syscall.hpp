#pragma once
#include <stdint.h>

// Syscall numbers (rax). Args in rdi, rsi, rdx (SysV/Linux-style).
#define SYS_WRITE 0     // write(fd, buf, len)  -> bytes written
#define SYS_YIELD 1     // yield()
#define SYS_EXIT  2     // exit()               -> never returns
#define SYS_READ  3     // read(fd, buf, len)   -> bytes read (non-blocking)
#define SYS_EXEC  4     // exec(name)           -> -1 on failure, else never returns
#define SYS_FORK  5     // fork()               -> child id in parent, 0 in child
#define SYS_WAIT  6     //wait()                -> bloacks until a child exits
#define SYS_OPEN  7     // open(path, flags)    -> fd (>=3), or -1
#define SYS_CLOSE 8     // close(fd)            -> 0, or -1
#define SYS_READDIR 9   // readdir(fd, idx, name) -> 1 entry / 0 end/ -1 (step 2c)

// open() flags (bitwise)
#define O_RDONLY 0x0
#define O_WRONLY 0x1
#define O_RDWR   0x2
#define O_CREAT  0x4
#define O_TRUNC  0x8
#define O_APPEND 0x10