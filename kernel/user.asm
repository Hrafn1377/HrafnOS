; user.asm - a tiny position-independent ring-3 program.
; Copied at runtime into a user page; only uses RIP-relative addressing and
; int 0x80, so it runs correctly wherever it's mapped. No privileged ops.
bits 64
section .text
global user_program_start
global user_program_end

user_program_start:
.loop:
    mov rax, 0              ; SYS_WRITE
    lea rdi, [rel .msg]     ; RIP-relative -> correct user vaddr at any load addr
    int 0x80                ; trap into the kernel from ring 3
    mov rcx, 0x8000000      ; crude delay
.delay:
    dec rcx
    jnz .delay
    jmp .loop
.msg: db "[ring3]", 0
user_program_end:
