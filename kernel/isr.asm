; isr.asm — CPU exception stubs (0–31) and hardware IRQ stubs (32–47).
;
; On an interrupt in long mode the CPU 16-byte-aligns RSP, then pushes:
;   SS, RSP, RFLAGS, CS, RIP, [error code for some exception vectors]
;
; Each stub normalizes the frame so the layout is identical for every
; vector — it ensures an 8-byte error-code slot exists (real or dummy)
; and then pushes the vector number. isr_common then saves the general
; registers and calls isr_handler(registers*). The push order here MUST
; match the `registers` struct in idt.cpp.
;
; isr_handler returns the stack pointer to resume on (rax). Normally that's
; the same stack; on a timer tick the scheduler returns a different task's
; stack, and `mov rsp, rax` performs the context switch.

extern isr_handler
global isr_stub_table

%macro ISR_NOERR 1
isr_stub_%1:
    push 0
    push %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr_stub_%1:
    push %1
    jmp isr_common
%endmacro

section .text
bits 64

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

ISR_NOERR 32     ; IRQ0  PIT timer
ISR_NOERR 33     ; IRQ1  keyboard
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47
ISR_NOERR 48     ; software yield / reschedule

global syscall_stub
syscall_stub:
    push 0
    push 0x80
    jmp isr_common

isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    cld
    mov rdi, rsp           ; arg1 = pointer to the saved register frame
    call isr_handler
    mov rsp, rax           ; resume on the returned stack (context switch point)

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16            ; discard vector number + error code
    iretq

section .rodata
isr_stub_table:
%assign v 0
%rep 49
    dq isr_stub_ %+ v
%assign v v+1
%endrep
