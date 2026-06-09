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

global prog_help_start
global prog_help_end
prog_help_start:
    incbin "help.elf"
prog_help_end:

global prog_clear_start
global prog_clear_end
prog_clear_start:
    incbin "clear.elf"
prog_clear_end:

global prog_echo_start
global prog_echo_end
prog_echo_start:
    incbin "echo.elf"
prog_echo_end: