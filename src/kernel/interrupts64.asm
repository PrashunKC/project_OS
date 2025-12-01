; 64-bit Interrupt Service Routines
; No pusha/popa in 64-bit, so we manually save/restore registers

bits 64

extern isr_handler
extern irq_handler

; Macro for ISRs without error code
%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0        ; Dummy error code
    push qword %1       ; Interrupt number
    jmp isr_common_stub
%endmacro

; Macro for ISRs with error code
%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1       ; Interrupt number
    jmp isr_common_stub
%endmacro

; Macro for IRQs
%macro IRQ 2
global irq%1
irq%1:
    push qword 0        ; Dummy error code
    push qword %2       ; Interrupt number
    jmp irq_common_stub
%endmacro

; CPU Exceptions (ISRs 0-31)
ISR_NOERR 0     ; Divide by zero
ISR_NOERR 1     ; Debug
ISR_NOERR 2     ; Non-maskable interrupt
ISR_NOERR 3     ; Breakpoint
ISR_NOERR 4     ; Overflow
ISR_NOERR 5     ; Bound Range Exceeded
ISR_NOERR 6     ; Invalid Opcode
ISR_NOERR 7     ; Device Not Available
ISR_ERR   8     ; Double Fault
ISR_NOERR 9     ; Coprocessor Segment Overrun
ISR_ERR   10    ; Invalid TSS
ISR_ERR   11    ; Segment Not Present
ISR_ERR   12    ; Stack-Segment Fault
ISR_ERR   13    ; General Protection Fault
ISR_ERR   14    ; Page Fault
ISR_NOERR 15    ; Reserved
ISR_NOERR 16    ; x87 Floating-Point Exception
ISR_ERR   17    ; Alignment Check
ISR_NOERR 18    ; Machine Check
ISR_NOERR 19    ; SIMD Floating-Point Exception
ISR_NOERR 20    ; Virtualization Exception
ISR_NOERR 21    ; Reserved
ISR_NOERR 22    ; Reserved
ISR_NOERR 23    ; Reserved
ISR_NOERR 24    ; Reserved
ISR_NOERR 25    ; Reserved
ISR_NOERR 26    ; Reserved
ISR_NOERR 27    ; Reserved
ISR_NOERR 28    ; Reserved
ISR_NOERR 29    ; Reserved
ISR_ERR   30    ; Security Exception
ISR_NOERR 31    ; Reserved

; Hardware Interrupts (IRQs 0-15 mapped to interrupts 32-47)
IRQ 0, 32       ; Timer
IRQ 1, 33       ; Keyboard
IRQ 2, 34       ; Cascade
IRQ 3, 35       ; COM2
IRQ 4, 36       ; COM1
IRQ 5, 37       ; LPT2
IRQ 6, 38       ; Floppy
IRQ 7, 39       ; LPT1
IRQ 8, 40       ; CMOS RTC
IRQ 9, 41       ; Free
IRQ 10, 42      ; Free
IRQ 11, 43      ; Free
IRQ 12, 44      ; PS/2 Mouse
IRQ 13, 45      ; FPU
IRQ 14, 46      ; Primary ATA
IRQ 15, 47      ; Secondary ATA

; Common ISR stub - manually saves all registers
isr_common_stub:
    ; DEBUG: Print '!' to top-left corner
    mov rax, 0x2F212F212F212F21
    mov [0xB8000], rax
    
    cld                 ; Clear direction flag (required by ABI)
    ; Save all general-purpose registers
    push rax
    push rcx
    push rdx
    push rbx
    push rsp
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Call C handler with pointer to interrupt frame in RDI
    mov rdi, rsp
    call isr_handler

    ; Restore all general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rsp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    ; Clean up error code and interrupt number
    add rsp, 16

    ; Return from interrupt
    iretq

; Common IRQ stub - manually saves all registers
irq_common_stub:
    ; DEBUG: Print '?' to top-left corner (next to '!')
    mov rax, 0x2F3F2F3F2F3F2F3F
    mov [0xB8002], rax

    cld                 ; Clear direction flag
    ; Save all general-purpose registers
    push rax
    push rcx
    push rdx
    push rbx
    push rsp
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Call C handler with pointer to interrupt frame in RDI
    mov rdi, rsp
    call irq_handler

    ; Restore all general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rsp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    ; Clean up error code and interrupt number
    add rsp, 16

    ; Return from interrupt
    iretq
