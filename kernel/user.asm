; user.asm - interactive ring-3 program.
; Reads a byte from the console, upper-cases letters, echoes it back, and
; exits on 'q'. Position-independent (RIP-relative only) so it runs wherever
; the kernel maps it. No privileged instructions.
bits 64
section .text
global user_program_start
global user_program_end

user_program_start:
.loop:
    ; n = read(0, buf, 1)
    mov rax, 3                ; SYS_READ
    xor rdi, rdi              ; fd 0
    lea rsi, [rel .buf]
    mov rdx, 1
    int 0x80
    test rax, rax
    jz .delay                 ; nothing available this poll

    mov al, [rel .buf]
    cmp al, 'q'
    je .quit
    cmp al, 'a'
    jb .write
    cmp al, 'z'
    ja .write
    sub al, 0x20              ; lowercase -> uppercase
    mov [rel .buf], al

.write:
    ; write(1, buf, 1)
    mov rax, 0                ; SYS_WRITE
    mov rdi, 1                ; fd 1
    lea rsi, [rel .buf]
    mov rdx, 1
    int 0x80

.delay:
    mov rcx, 0x200000
.d2:
    dec rcx
    jnz .d2
    jmp .loop

.quit:
    mov rax, 2                ; SYS_EXIT
    int 0x80                  ; never returns

.buf: db 0
user_program_end:
