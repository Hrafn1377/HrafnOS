; user.asm - two ring-3 programs for the address-space isolation demo.
; Each loops forever writing its own label. Position-independent so each can
; be mapped at the SAME virtual address in its own (separate) address space.
bits 64
section .text

global user_a_start
global user_a_end
global user_b_start
global user_b_end

user_a_start:
.loop:
    mov rax, 0              ; SYS_WRITE
    mov rdi, 1              ; fd 1
    lea rsi, [rel .msg]
    mov rdx, 3
    int 0x80
    mov rcx, 0x6000000
.d:
    dec rcx
    jnz .d
    jmp .loop
.msg: db "[A]"
user_a_end:

user_b_start:
.loop:
    mov rax, 0
    mov rdi, 1
    lea rsi, [rel .msg]
    mov rdx, 3
    int 0x80
    mov rcx, 0x6000000
.d:
    dec rcx
    jnz .d
    jmp .loop
.msg: db "[B]"
user_b_end:
