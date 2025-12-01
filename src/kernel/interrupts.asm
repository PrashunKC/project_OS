[bits 64]

; Defined in idt.c
extern idt_ptr

; Defined in isr.c
extern isr_handler
extern irq_handler

; Defined in syscall.c
extern syscall_handler

global idt_load
global isr0
global isr1
global isr2
global isr3
global isr4
global isr5
global isr6
global isr7
global isr8
global isr9
global isr10
global isr11
global isr12
global isr13
global isr14
global isr15
global isr16
global isr17
global isr18
global isr19
global isr20
global isr21
global isr22
global isr23
global isr24
global isr25
global isr26
global isr27
global isr28
global isr29
global isr30
global isr31

global irq0
global irq1
global irq2
global irq3
global irq4
global irq5
global irq6
global irq7
global irq8
global irq9
global irq10
global irq11
global irq12
global irq13
global irq14
global irq15

; Syscall interrupt
global isr128

; Load the IDT
idt_load:
    lidt [idt_ptr]
    ret

; Common ISR stub for 64-bit mode
isr_common_stub:
    ; Save all general purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to stack frame as first argument (RDI in System V ABI)
    mov rdi, rsp
    
    ; Align stack to 16 bytes (required by System V ABI)
    mov rbp, rsp
    and rsp, ~0xF
    
    call isr_handler
    
    ; Restore stack
    mov rsp, rbp

    ; Restore all registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove error code and interrupt number from stack
    add rsp, 16

    iretq

; Common IRQ stub for 64-bit mode
irq_common_stub:
    ; Save all general purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to stack frame as first argument (RDI in System V ABI)
    mov rdi, rsp
    
    ; Align stack to 16 bytes
    mov rbp, rsp
    and rsp, ~0xF
    
    call irq_handler
    
    ; Restore stack
    mov rsp, rbp

    ; Restore all registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove error code and interrupt number from stack
    add rsp, 16

    iretq

; Macros to define ISRs
%macro ISR_NOERRCODE 1
isr%1:
    push qword 0        ; Push dummy error code
    push qword %1       ; Push interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
isr%1:
    push qword %1       ; Push interrupt number (error code already on stack)
    jmp isr_common_stub
%endmacro

%macro IRQ 2
irq%1:
    push qword 0        ; Push dummy error code
    push qword %2       ; Push interrupt number
    jmp irq_common_stub
%endmacro

; Define ISRs
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; Define IRQs
IRQ 0,  32
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; ============================================================================
; Syscall interrupt (INT 0x80)
; ============================================================================

; Syscall stub - similar to ISR but calls syscall_handler
syscall_common_stub:
    ; Save all general purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to stack frame as first argument (RDI in System V ABI)
    mov rdi, rsp
    
    ; Align stack to 16 bytes (required by System V ABI)
    mov rbp, rsp
    and rsp, ~0xF
    
    call syscall_handler
    
    ; Restore stack
    mov rsp, rbp

    ; Restore all registers (RAX contains return value, restored from stack frame)
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax     ; Return value is in RAX (set by syscall_handler in the stack frame)

    ; Remove error code and interrupt number from stack
    add rsp, 16

    iretq

; INT 0x80 - Syscall
isr128:
    push qword 0        ; Push dummy error code
    push qword 128      ; Push interrupt number
    jmp syscall_common_stub
