bits 64

global idt64_load

section .text
idt64_load:
    ; RDI contains pointer to IDT pointer structure
    lidt [rdi]
    ret
