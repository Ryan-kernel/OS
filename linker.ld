ENTRY(_start)

SECTIONS
{
    . = 0x10000;

    .text ALIGN(16) : {
        *(.text)
    }

    .rodata ALIGN(16) : {
        *(.rodata)
    }

    .data ALIGN(16) : {
        *(.data)
    }

    .bss ALIGN(16) : {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.reloc)
        *(.eh_frame)
        *(.comment)
    }
}