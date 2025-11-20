ENTRY(start)
OUTPUT_FORMAT("binary")
PHYSICAL_BASE = 0x100000;

SECTIONS
{
    . = PHYSICAL_BASE;

    .entry :
    {
        *(.entry)
    }

    .text :
    {
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
