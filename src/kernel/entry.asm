bits 32
global _start
extern start

section .entry
_start:
    ; Set up stack for kernel
    mov esp, 0x200000
    mov ebp, esp

    call start
    cli
    hlt
