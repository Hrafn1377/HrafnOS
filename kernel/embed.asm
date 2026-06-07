; Embeds the compiled user ELF into the kernel image as raw bytes.
section .rodata
global user_elf_start
global user_elf_end
user_elf_start:
    incbin "user.elf"
user_elf_end:
