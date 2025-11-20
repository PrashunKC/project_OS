bits 16

section .entry

extern cstart_
global entry

entry:
    cli
    
    ; Setup segments to 0 so that we can access data at absolute addresses
    ; (The linker assumes a flat binary starting at 0x20000)
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov bp, sp

    ; Save boot drive
    mov eax, g_BootDrive
    mov [eax], dl

    call EnableA20
    call LoadGDT

    ; Set protected mode bit
    mov eax, cr0
    or al, 1
    mov cr0, eax

    ; Far jump to protected mode
    jmp dword 0x08:pmode

[bits 32]
pmode:
    ; Setup segments
    mov ax, 0x10
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Debug: Write 'P' to top-left corner (Green on Black)
    mov byte [0xB8000], 'P'
    mov byte [0xB8001], 0x02

    ; Setup stack in protected mode
    mov ebp, 0x90000
    mov esp, ebp

    ; Call C kernel
    xor edx, edx
    mov dl, [g_BootDrive]
    push edx
    call cstart_

    cli
    hlt

[bits 16]
EnableA20:
    ; Try BIOS method first
    mov ax, 0x2401
    int 0x15
    ret

LoadGDT:
    mov eax, g_GDTDesc
    lgdt [eax]
    ret

g_GDT:
    ; Null descriptor
    dq 0

    ; 32-bit Code Segment (0x08)
    dw 0xFFFF
    dw 0
    db 0
    db 10011010b
    db 11001111b
    db 0

    ; 32-bit Data Segment (0x10)
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b
    db 11001111b
    db 0

    ; 16-bit Code Segment (0x18)
    dw 0xFFFF
    dw 0
    db 2
    db 10011010b
    db 00001111b
    db 0

    ; 16-bit Data Segment (0x20)
    dw 0xFFFF
    dw 0
    db 2
    db 10010010b
    db 00001111b
    db 0

g_GDT_end:

g_GDTDesc:
    dw g_GDT_end - g_GDT - 1
    dd g_GDT

g_BootDrive: db 0
