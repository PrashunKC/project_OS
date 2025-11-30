bits 16

section .entry

extern cstart_
global entry

; Stage2 is loaded at segment 0x2000, offset 0x0000 (linear address 0x20000)
STAGE2_SEGMENT equ 0x2000
BOOT_DRIVE_OFFSET equ 0x7F00   ; Store boot drive at a known offset within segment

entry:
    cli
    
    ; At this point:
    ; - CS = 0x2000 (from stage1's far jump)
    ; - DL = boot drive number
    ; - We need to set up other segments
    
    ; Setup data segment to match code segment
    mov ax, STAGE2_SEGMENT
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0xFFF0      ; Stack at top of segment (linear 0x2FFF0)
    mov bp, sp
    
    ; Save boot drive at a known location
    mov [BOOT_DRIVE_OFFSET], dl

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
    ; Setup segments with 32-bit data selector
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

    ; Call C kernel - get boot drive from known location
    xor edx, edx
    mov dl, [0x20000 + BOOT_DRIVE_OFFSET]  ; Linear address = segment base + offset
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
    ; Get our current IP to calculate absolute position
    call .getip
.getip:
    pop bx                      ; BX = IP of .getip
    sub bx, .getip - GDTDesc    ; BX = offset of GDTDesc within segment
    lgdt [bx]
    ret

; GDT placed right after LoadGDT for proximity
align 8
GDTStart:
    ; Null descriptor
    dq 0

    ; 32-bit Code Segment (0x08) - base 0, limit 4GB
    dw 0xFFFF       ; limit low
    dw 0            ; base low
    db 0            ; base mid
    db 10011010b    ; access: present, ring0, code, executable, readable
    db 11001111b    ; flags: 4KB granularity, 32-bit + limit high
    db 0            ; base high

    ; 32-bit Data Segment (0x10) - base 0, limit 4GB
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b    ; access: present, ring0, data, writable
    db 11001111b
    db 0

    ; 16-bit Code Segment (0x18) - base at 0x20000
    dw 0xFFFF
    dw 0
    db 2            ; base mid = 0x02 (for 0x020000)
    db 10011010b
    db 00001111b    ; 16-bit mode
    db 0

    ; 16-bit Data Segment (0x20) - base at 0x20000
    dw 0xFFFF
    dw 0
    db 2
    db 10010010b
    db 00001111b
    db 0

GDTEnd:

; GDT Descriptor - placed right after GDT
GDTDesc:
    dw GDTEnd - GDTStart - 1        ; limit (size - 1)
    dd GDTStart                      ; linear address of GDT (linker provides 0x20xxx)
