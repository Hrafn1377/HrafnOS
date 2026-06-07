ENTRY(_start)

SECTIONS
{
    . = 0x8000000000;          /* user base: PML4[1], private per process */

    .text   : { *(.text*)   }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*)   }
    .bss    : { *(.bss*) *(COMMON) }
}
