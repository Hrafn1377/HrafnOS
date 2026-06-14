; The ramdisk: each program's ELF embedded as raw bytes, bracketed by symbols.
section .rodata

global prog_one_start
global prog_one_end
prog_one_start:
    incbin "one.elf"
prog_one_end:

global prog_two_start
global prog_two_end
prog_two_start:
    incbin "two.elf"
prog_two_end:

global prog_huginn_start
global prog_huginn_end
prog_huginn_start:
    incbin "huginn.elf"
prog_huginn_end:

