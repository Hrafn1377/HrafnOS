global _start
extern kmain

section .multiboot
align 8
header_start:
    dd 0xE85250D6
    dd 0
    dd header_end - header_start
    dd -(0xE85250D6 + 0 + (header_end - header_start))
    dw 0
    dw 0
    dd 8
header_end:

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
align 8
global g_heap_next
g_heap_next:
    resq 1

section .data
align 16
gdt:
    dq 0x0000000000000000
    dq 0x00af9a000000ffff
    dq 0x00af92000000ffff
gdt_ptr:
    dw 23
    dq gdt
    dq 0                   ; padding

section .text
bits 64
_start:
    mov rsp, stack_top
    and rsp, -16
    lgdt [gdt_ptr]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor ax, ax
    mov fs, ax
    mov gs, ax
    call kmain
.halt:
    hlt
    jmp .halt