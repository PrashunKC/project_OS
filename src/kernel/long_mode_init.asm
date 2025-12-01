global long_mode_start
extern start

section .text
bits 32

long_mode_start:
    ; Check if CPU supports Long Mode
    call check_cpuid
    call check_long_mode

    ; Set up paging
    call setup_page_tables
    call enable_paging

    ; Load 64-bit GDT
    lgdt [gdt64.pointer]

    ; Jump to 64-bit code segment
    jmp gdt64.code_segment:long_mode_entry

bits 64
long_mode_entry:
    ; Update data segment selectors
    mov ax, gdt64.data_segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Debug: Write "LM" to confirm Long Mode active
    mov word [0xb8000], 0x4f4c  ; 'L' with white on red
    mov word [0xb8002], 0x4f4d  ; 'M' with white on red

    ; Call kernel entry point
    ; Note: System V AMD64 ABI passes first argument in RDI
    ; We don't have valid bootDrive passed from stage2 yet, so pass 0
    xor rdi, rdi 
    call start

    ; Halt if kernel returns
    cli
    hlt

bits 32
check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    mov al, '1'
    jmp error

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    ret
.no_long_mode:
    mov al, '2'
    jmp error

setup_page_tables:
    ; Map first 2MB to 0x0 using 2MB huge page
    
    ; P4L[0] -> P3L
    mov eax, page_table_l3
    or eax, 0b11 ; present + writable
    mov [page_table_l4], eax

    ; P3L[0] -> P2L
    mov eax, page_table_l2
    or eax, 0b11 ; present + writable
    mov [page_table_l3], eax

    ; P2L[0] -> 2MB page at 0x0
    mov eax, 0x0
    or eax, 0b10000011 ; present + writable + huge
    mov [page_table_l2], eax

    ret

enable_paging:
    ; Load P4L to CR3
    mov eax, page_table_l4
    mov cr3, eax

    ; Enable PAE-flag in CR4 (Bit 5)
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Enable Long Mode in EFER MSR (0xC0000080)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable Paging in CR0 (Bit 31)
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ret

error:
    ; Print "ERR: X" where X is in AL
    mov dword [0xb8000], 0x4f524f45
    mov dword [0xb8004], 0x4f3a4f52
    mov dword [0xb8008], 0x4f204f20
    mov byte  [0xb800a], al
    hlt

section .rodata
gdt64:
    dq 0 ; Zero entry
.code_segment: equ $ - gdt64
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53) ; Code segment
.data_segment: equ $ - gdt64
    dq (1 << 44) | (1 << 47) | (1 << 41) ; Data segment
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .bss
align 4096
page_table_l4:
    resb 4096
page_table_l3:
    resb 4096
page_table_l2:
    resb 4096
