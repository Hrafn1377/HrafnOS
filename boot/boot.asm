; boot.asm — HrafnOS entry point
; Entered by GRUB via Multiboot2 in 32-bit protected mode (paging off).
; Sets up long mode, then jumps to the 64-bit kernel (kmain).

global _start
extern kmain

; ---------------------------------------------------------------------------
; Multiboot2 header (must be 8-byte aligned, within first 32 KiB of the file)
; ---------------------------------------------------------------------------
section .multiboot
align 8
header_start:
    dd 0xE85250D6                              ; magic
    dd 0                                       ; architecture: 0 = i386 (32-bit)
    dd header_end - header_start               ; header length
    dd -(0xE85250D6 + 0 + (header_end - header_start))  ; checksum

    ; framebuffer request tag (ask GRUB for a linear RGB framebuffer)
    align 8
    dw 5                                       ; type = framebuffer
    dw 0                                       ; flags (0 = required)
    dd 20                                      ; size
    dd 1024                                    ; preferred width
    dd 768                                     ; preferred height
    dd 32                                      ; preferred bpp

    align 8
    dw 0                                       ; end tag: type
    dw 0                                       ; end tag: flags
    dd 8                                       ; end tag: size
header_end:

; ---------------------------------------------------------------------------
; BSS: page tables + stack
; ---------------------------------------------------------------------------
section .bss
align 4096
pml4:   resb 4096
pdpt:   resb 4096
pd:     resb 4096
align 16
stack_bottom:
    resb 262144                                ; 256 KiB kernel stack
stack_top:

; ---------------------------------------------------------------------------
; 64-bit GDT
; ---------------------------------------------------------------------------
section .rodata
align 16
gdt64:
    dq 0x0000000000000000                      ; 0x00: null
    dq 0x00af9a000000ffff                      ; 0x08: 64-bit ring-0 code
    dq 0x00af92000000ffff                      ; 0x10: ring-0 data
gdt64_ptr:
    dw $ - gdt64 - 1                           ; limit
    dq gdt64                                    ; base

; ---------------------------------------------------------------------------
; 32-bit entry: transition to long mode
; ---------------------------------------------------------------------------
section .text
bits 32
_start:
    mov esp, stack_top                         ; temporary 32-bit stack
    mov edi, ebx                               ; preserve Multiboot2 info ptr
                                               ; (arrives as kmain's 1st arg)

    ; PML4[0] -> PDPT
    mov eax, pdpt
    or  eax, 0b11                              ; present | writable
    mov [pml4], eax

    ; PDPT[0] -> PD
    mov eax, pd
    or  eax, 0b11
    mov [pdpt], eax

    ; PD: identity-map the first 1 GiB using 512 x 2 MiB pages.
    ; Covers kernel (2 MiB), stack, and heap (32–112 MiB).
    mov ecx, 0
    mov eax, 0b10000011                        ; phys 0 | present | writable | PS
.map_pd:
    mov [pd + ecx*8], eax
    mov dword [pd + ecx*8 + 4], 0              ; high dword (phys < 4 GiB)
    add eax, 0x200000                          ; next 2 MiB frame
    inc ecx
    cmp ecx, 512
    jne .map_pd

    ; Load CR3 with the PML4
    mov eax, pml4
    mov cr3, eax

    ; Enable PAE (CR4.PAE)
    mov eax, cr4
    or  eax, 1 << 5
    mov cr4, eax

    ; Set long-mode-enable (EFER.LME, MSR 0xC0000080 bit 8)
    mov ecx, 0xC0000080
    rdmsr
    or  eax, 1 << 8
    wrmsr

    ; Enable paging (CR0.PG) — activates long mode
    mov eax, cr0
    or  eax, 1 << 31
    mov cr0, eax

    ; Load 64-bit GDT, far-jump to reload CS into the 64-bit code segment
    lgdt [gdt64_ptr]
    jmp 0x08:long_mode_start

; ---------------------------------------------------------------------------
; 64-bit kernel entry
; ---------------------------------------------------------------------------
bits 64
long_mode_start:
    mov ax, 0x10                               ; data selector
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor ax, ax
    mov fs, ax
    mov gs, ax

    mov rsp, stack_top
    and rsp, -16                               ; 16-byte align per SysV ABI

    call kmain
.halt:
    hlt
    jmp .halt

; ---------------------------------------------------------------------------
; Heap pointer + accessors (linked against by heap.cpp)
; ---------------------------------------------------------------------------
section .data
align 16
heap_ptr:
    dq 0

section .text
global asm_get_heap
asm_get_heap:
    mov rax, [heap_ptr]
    ret

global asm_set_heap
asm_set_heap:
    mov [heap_ptr], rdi
    ret