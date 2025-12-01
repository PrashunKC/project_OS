ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS
{
    . = 1M;

    .text ALIGN(4) : AT(ADDR(.text))
    {
        *(.multiboot)  /* Multiboot header first */
        . = ALIGN(16);
        *(.text)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata))
    {
        *(.rodata*)
        *(.eh_frame)
    }

    .data ALIGN(4K) : AT(ADDR(.data))
    {
        *(.data)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss))
    {
        *(COMMON)
        *(.bss)
    }
}
