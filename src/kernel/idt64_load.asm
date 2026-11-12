bits 64

; Load an IDT pointer supplied by the alternate 64-bit C implementation.
global idt64_load

section .text
idt64_load:
    ; RDI contains pointer to IDT pointer structure
    lidt [rdi]
    ret
