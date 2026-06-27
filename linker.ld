ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .multiboot :
    {
        KEEP(*(.multiboot))
    }

    .text :
    {
        *(.text)
        _code_end = .;
    }

    .data :
    {
        *(.data)
        *(.rodata)
        _data_end = .;
    }

    .bss :
    {
        *(.COMMON)
        *(.bss)
        _bss_end = .;
    }
}