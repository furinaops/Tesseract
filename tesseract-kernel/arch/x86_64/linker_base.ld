ENTRY(_start)

SECTIONS
{
    . = 16M;

    .text :
    {
        *(.text)
        *(.text.*)
    }

    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data :
    {
        *(.data)
        *(.data.*)
    }

    .bss :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /DISCARD/ : { *(.comment) *(.note.*) *(.eh_frame) *(.debug*) }
}
