ENTRY(_start)

SECTIONS
{
    . = 2M;

    .multiboot : { *(.multiboot) }
    .text      : { *(.text*) }
    .rodata    : { *(.rodata*) }
    .data      : { *(.data*) }
    .bss       : {
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        heap_ptr_loc = .;
        . += 8;
    }
}