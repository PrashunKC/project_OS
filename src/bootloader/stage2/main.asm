bits 16

section .entry

extern cstart_
global entry

; Stage2 is loaded at segment 0x2000, offset 0x0000 (linear address 0x20000)
STAGE2_SEGMENT equ 0x2000
BOOT_DRIVE_OFFSET equ 0x7F00   ; Store boot drive at a known offset within segment

; VESA info buffer location (in low memory, accessible in real mode)
VESA_INFO_BUFFER equ 0x7000    ; Linear address for VBE info
VESA_MODE_BUFFER equ 0x7200    ; Linear address for mode info

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

    ; Set up VESA graphics mode before protected mode
    call SetupVESA

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


; ============================================================================
; VESA VBE Setup (called in real mode before protected mode)
; ============================================================================

SetupVESA:
    push es
    push di
    push si
    push bx
    
    ; First, get VBE controller info
    ; ES:DI = pointer to VbeInfoBlock
    xor ax, ax
    mov es, ax
    mov di, VESA_INFO_BUFFER
    
    ; Set signature to "VBE2" to request VBE 2.0+ info
    mov dword [es:di], 'VBE2'
    
    mov ax, 0x4F00      ; VBE Function 00h - Get Controller Info
    int 0x10
    
    ; Check if VBE is supported
    cmp ax, 0x004F
    jne .vesa_failed
    
    ; Verify signature changed to "VESA"
    cmp dword [es:di], 'VESA'
    jne .vesa_failed
    
    ; Now enumerate modes to find a good one
    ; Get mode list pointer (at offset 14 in VbeInfoBlock)
    mov si, [es:di + 14]    ; Offset of mode list
    mov ax, [es:di + 16]    ; Segment of mode list
    mov fs, ax              ; FS:SI = mode list
    
.find_mode:
    mov cx, [fs:si]         ; Get mode number
    cmp cx, 0xFFFF          ; End of list?
    je .try_fallback
    
    ; Get mode info for this mode
    push cx
    push si
    
    xor ax, ax
    mov es, ax
    mov di, VESA_MODE_BUFFER
    mov ax, 0x4F01          ; VBE Function 01h - Get Mode Info
    int 0x10
    
    pop si
    pop cx
    
    cmp ax, 0x004F
    jne .next_mode
    
    ; Check mode attributes (offset 0)
    ; Bit 0: Mode supported
    ; Bit 4: Graphics mode
    ; Bit 7: Linear framebuffer available
    mov ax, [es:di]
    and ax, 0x0091          ; Check bits 0, 4, 7
    cmp ax, 0x0091
    jne .next_mode
    
    ; Check resolution - look for 1024x768 or 800x600 with 32bpp
    mov ax, [es:di + 18]    ; Width
    cmp ax, 1024
    jne .check_800
    mov ax, [es:di + 20]    ; Height
    cmp ax, 768
    jne .check_800
    mov al, [es:di + 25]    ; BPP
    cmp al, 32
    je .found_mode
    
.check_800:
    mov ax, [es:di + 18]
    cmp ax, 800
    jne .check_640
    mov ax, [es:di + 20]
    cmp ax, 600
    jne .check_640
    mov al, [es:di + 25]
    cmp al, 32
    je .found_mode
    
.check_640:
    mov ax, [es:di + 18]
    cmp ax, 640
    jne .next_mode
    mov ax, [es:di + 20]
    cmp ax, 480
    jne .next_mode
    mov al, [es:di + 25]
    cmp al, 32
    je .found_mode
    cmp al, 24
    je .found_mode
    
.next_mode:
    add si, 2
    jmp .find_mode

.try_fallback:
    ; Try standard VBE mode numbers
    mov cx, 0x118           ; 1024x768x32
    call .try_set_mode
    jc .try_115
    jmp .mode_set_done
    
.try_115:
    mov cx, 0x115           ; 800x600x32
    call .try_set_mode
    jc .try_112
    jmp .mode_set_done
    
.try_112:
    mov cx, 0x112           ; 640x480x32
    call .try_set_mode
    jc .vesa_failed
    jmp .mode_set_done

.found_mode:
    ; CX still has the mode number
    ; Set the mode with linear framebuffer
    mov bx, cx
    or bx, 0x4000           ; Bit 14 = use linear framebuffer
    mov ax, 0x4F02           ; VBE Function 02h - Set Mode
    int 0x10
    cmp ax, 0x004F
    jne .vesa_failed

.mode_set_done:
    ; Save mode info to a fixed low memory location for C code to read
    ; We'll use 0x8100 as a safe location (after FB_INFO at 0x8000)
    push ds
    push es
    
    xor ax, ax
    mov ds, ax
    mov si, VESA_MODE_BUFFER    ; Source: mode info from BIOS
    mov di, 0x8100              ; Destination: fixed location
    mov es, ax
    
    mov cx, 128                 ; Copy 256 bytes
    rep movsw
    
    ; Mark VESA as available at 0x80FF
    mov byte [0x80FF], 1
    
    pop es
    pop ds
    jmp .vesa_done

.try_set_mode:
    ; Try to set mode in CX
    ; First get mode info
    push cx
    xor ax, ax
    mov es, ax
    mov di, VESA_MODE_BUFFER
    mov ax, 0x4F01
    int 0x10
    pop cx
    
    cmp ax, 0x004F
    jne .try_set_failed
    
    ; Check if mode is valid
    mov ax, [es:di]
    and ax, 0x0091
    cmp ax, 0x0091
    jne .try_set_failed
    
    ; Set the mode
    mov bx, cx
    or bx, 0x4000
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .try_set_failed
    
    clc                     ; Success
    ret
    
.try_set_failed:
    stc                     ; Failure
    ret

.vesa_failed:
    ; VESA not available, stay in text mode
    ; Mark at 0x80FF as not available
    xor ax, ax
    mov es, ax
    mov byte [es:0x80FF], 0
    
.vesa_done:
    pop bx
    pop si
    pop di
    pop es
    ret
