ENTRY(entry)
OUTPUT_FORMAT("binary")
PHYSICAL_BASE = 0x20000;

SECTIONS
{
    . = PHYSICAL_BASE;

    .text :
    {
        *(.entry)
        *(.text)
    }

    .data :
    {
        *(.data)
    }

    .rodata :
    {
        *(.rodata)
    }

    .bss :
    {
        *(.bss)
    }
}
