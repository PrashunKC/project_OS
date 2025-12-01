bits 32

; Multiboot constants
MULTIBOOT_MAGIC        equ 0x1BADB002
MULTIBOOT_PAGE_ALIGN   equ 1 << 0
MULTIBOOT_MEMORY_INFO  equ 1 << 1
MULTIBOOT_VIDEO_MODE   equ 1 << 2
MULTIBOOT_FLAGS        equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO
MULTIBOOT_CHECKSUM     equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

; Page table constants
PAGE_PRESENT    equ 1 << 0
PAGE_WRITABLE   equ 1 << 1
PAGE_HUGE       equ 1 << 7

global _start
extern start64

; ============================================================================
; MULTIBOOT HEADER SECTION
; ============================================================================
section .multiboot
align 4

; Multiboot header (must be in first 8KB of kernel)
multiboot_header:
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

; ============================================================================
; 32-BIT ENTRY POINT
; ============================================================================
section .text
bits 32

_start:
    ; Disable interrupts
    cli
    
    ; Save multiboot info (EAX = magic, EBX = info pointer)
    mov edi, eax            ; Save magic in EDI
    mov esi, ebx            ; Save multiboot info in ESI
    
    ; Set up temporary 32-bit stack
    mov esp, stack_top
    mov ebp, esp

    ; Check for CPUID support
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21        ; Flip ID bit
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    xor eax, ecx
    jz .no_cpuid
    
    ; Check for extended CPUID
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    
    ; Check for Long Mode support
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29       ; LM bit
    jz .no_long_mode
    
    ; ========================================================================
    ; Set up identity-mapped page tables for first 4GB (using 2MB pages)
    ; ========================================================================
    
    ; Clear ALL page table area (24KB: pml4 + pdpt + pd_table)
    mov edi, pml4_table
    xor eax, eax
    mov ecx, 6144           ; 24KB = 6144 dwords (4096 + 4096 + 16384 bytes)
    rep stosd
    
    ; Set up PML4[0] -> PDPT
    mov eax, pdpt_table
    or eax, PAGE_PRESENT | PAGE_WRITABLE
    mov [pml4_table], eax
    
    ; Set up PD entries for first 4GB (using 2MB huge pages)
    ; Each PD entry maps 2MB, each PD has 512 entries = 1GB
    ; We need 4 PDPTs to map 4GB total (for framebuffer access)
    
    ; Set up PDPT[0] -> PD (first 1GB: 0x00000000 - 0x3FFFFFFF)
    mov eax, pd_table
    or eax, PAGE_PRESENT | PAGE_WRITABLE
    mov [pdpt_table], eax
    
    ; Set up PDPT[1] -> PD+4096 (second 1GB: 0x40000000 - 0x7FFFFFFF)
    mov eax, pd_table + 4096
    or eax, PAGE_PRESENT | PAGE_WRITABLE
    mov [pdpt_table + 8], eax
    
    ; Set up PDPT[2] -> PD+8192 (third 1GB: 0x80000000 - 0xBFFFFFFF)
    mov eax, pd_table + 8192
    or eax, PAGE_PRESENT | PAGE_WRITABLE
    mov [pdpt_table + 16], eax
    
    ; Set up PDPT[3] -> PD+12288 (fourth 1GB: 0xC0000000 - 0xFFFFFFFF)
    mov eax, pd_table + 12288
    or eax, PAGE_PRESENT | PAGE_WRITABLE
    mov [pdpt_table + 24], eax
    
    ; Now fill all 4 Page Directory tables (2048 entries total for 4GB)
    ; Using 2MB huge pages: each entry maps 2MB of physical memory
    mov edi, pd_table
    mov eax, PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE  ; 0x83
    mov ecx, 2048           ; 512 entries * 4 PDs = 2048 entries for 4GB
.map_pd:
    mov dword [edi], eax    ; Low 32 bits of entry
    mov dword [edi + 4], 0  ; High 32 bits = 0 (physical addr < 4GB)
    add eax, 0x200000       ; Next 2MB physical address
    add edi, 8              ; Next 8-byte entry
    dec ecx
    jnz .map_pd
    
    ; ========================================================================
    ; Enable PAE (Physical Address Extension)
    ; ========================================================================
    mov eax, cr4
    or eax, 1 << 5          ; PAE bit
    mov cr4, eax
    
    ; ========================================================================
    ; Load PML4 address into CR3
    ; ========================================================================
    mov eax, pml4_table
    mov cr3, eax
    
    ; ========================================================================
    ; Enable Long Mode in EFER MSR
    ; ========================================================================
    mov ecx, 0xC0000080     ; EFER MSR
    rdmsr
    or eax, 1 << 8          ; LME bit (Long Mode Enable)
    wrmsr
    
    ; ========================================================================
    ; Enable Paging (this activates Long Mode)
    ; ========================================================================
    mov eax, cr0
    or eax, 1 << 31         ; PG bit
    mov cr0, eax
    
    ; ========================================================================
    ; Load 64-bit GDT and far jump to 64-bit code
    ; ========================================================================
    lgdt [gdt64_ptr]
    jmp 0x08:long_mode_start

.no_cpuid:
    mov esi, msg_no_cpuid
    jmp .error
    
.no_long_mode:
    mov esi, msg_no_longmode
    jmp .error

.error:
    ; Print error message to VGA
    mov edi, 0xB8000
.print_loop:
    lodsb
    test al, al
    jz .halt
    mov ah, 0x4F            ; White on red
    stosw
    jmp .print_loop
.halt:
    cli
    hlt
    jmp .halt

msg_no_cpuid:    db "ERROR: CPUID not supported!", 0
msg_no_longmode: db "ERROR: Long Mode not supported!", 0

; ============================================================================
; 64-BIT LONG MODE ENTRY
; ============================================================================
bits 64

long_mode_start:
    ; Reload segment registers with 64-bit data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up 64-bit stack
    mov rsp, stack_top
    mov rbp, rsp
    
    ; Clear direction flag
    cld
    
    ; RDI already contains multiboot magic (from 32-bit EDI)
    ; RSI already contains multiboot info ptr (from 32-bit ESI)
    ; These are the first two arguments in System V AMD64 ABI
    
    ; Call 64-bit kernel main
    call start64
    
    ; Halt if kernel returns
    cli
.halt64:
    hlt
    jmp .halt64

; ============================================================================
; 64-BIT GDT
; ============================================================================
section .rodata
align 16

gdt64:
    ; Null descriptor
    dq 0
    
    ; Code segment (64-bit)
    ; Base=0, Limit=0xFFFFF, Access=0x9A, Flags=0xA (L=1, D=0)
    dq 0x00AF9A000000FFFF
    
    ; Data segment (64-bit)
    ; Base=0, Limit=0xFFFFF, Access=0x92, Flags=0x8 (L=0, D=0)
    dq 0x00CF92000000FFFF

gdt64_ptr:
    dw $ - gdt64 - 1        ; Limit
    dq gdt64                ; Base

; ============================================================================
; PAGE TABLES (must be 4KB aligned)
; ============================================================================
section .bss
align 4096

pml4_table:
    resb 4096
pdpt_table:
    resb 4096
pd_table:
    resb 16384              ; 4 Page Directory tables for 4GB mapping
pt_table:
    resb 4096

; ============================================================================
; STACK (16KB)
; ============================================================================
align 16
stack_bottom:
    resb 16384
stack_top:
