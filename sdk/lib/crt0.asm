; NBOS C Runtime Startup
; This is the entry point for NBOS programs

bits 64
section .text

global _start
extern main

_start:
    ; Set up stack frame
    push rbp
    mov rbp, rsp
    
    ; Clear BSS (if needed)
    ; For now, skip this
    
    ; Call main()
    xor edi, edi        ; argc = 0
    xor esi, esi        ; argv = NULL
    call main
    
    ; Exit with return value from main
    mov rdi, rax        ; Return value
    mov rax, 0          ; SYS_EXIT
    int 0x80
    
    ; Should never reach here
    cli
.halt:
    hlt
    jmp .halt
