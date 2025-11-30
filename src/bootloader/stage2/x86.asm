bits 32

section .text

global x86_Video_WriteCharTeletype
global x86_Disk_Reset
global x86_Disk_Read
global x86_Disk_GetDriveParams

; ------------------------------------------------------------------------------
; x86_Video_WriteCharTeletype
; void x86_Video_WriteCharTeletype(char c, uint8_t page);
; ------------------------------------------------------------------------------
x86_Video_WriteCharTeletype:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov al, [ebp + 8]   ; c
    mov bh, [ebp + 12]  ; page
    mov ah, 0x0E

    ; Save registers for Real Mode
    mov [g_RegAX], ax
    mov [g_RegBX], bx

    call x86_EnterRealMode
    
    [bits 16]
    a32 mov eax, [g_RegAX] ; Load address of g_RegAX? No, g_RegAX is the address.
    ; Wait, mov eax, g_RegAX loads the immediate address.
    ; We want to load the value from that address.
    ; But g_RegAX is a label.
    ; mov ax, [g_RegAX] should work if we use a32.
    
    a32 mov ax, [g_RegAX]
    a32 mov bx, [g_RegBX]
    int 0x10
    
    call x86_EnterProtectedMode
    [bits 32]

    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; ------------------------------------------------------------------------------
; x86_Disk_Reset
; bool x86_Disk_Reset(uint8_t drive);
; ------------------------------------------------------------------------------
x86_Disk_Reset:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov dl, [ebp + 8]   ; drive
    mov ah, 0

    mov [g_RegDX], dx
    mov [g_RegAX], ax

    call x86_EnterRealMode
    
    [bits 16]
    a32 mov dx, [g_RegDX]
    a32 mov ax, [g_RegAX]
    stc
    int 0x13
    
    pushf
    pop ax
    a32 mov [g_RegFlags], ax
    
    call x86_EnterProtectedMode
    [bits 32]

    ; Check carry flag (bit 0 = error)
    movzx eax, word [g_RegFlags]
    test al, 1
    jnz .error
    mov eax, 1
    jmp .done
.error:
    xor eax, eax
.done:

    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; ------------------------------------------------------------------------------
; x86_Disk_Read
; bool x86_Disk_Read(uint8_t drive, uint16_t cylinder, uint16_t sector, uint16_t head, uint8_t count, void* dataOut);
; ------------------------------------------------------------------------------
x86_Disk_Read:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov dl, [ebp + 8]   ; drive
    mov ch, [ebp + 12]  ; cylinder (low)
    mov cl, [ebp + 13]  ; cylinder (high)
    shl cl, 6
    mov al, [ebp + 16]  ; sector
    and al, 0x3F
    or cl, al
    mov dh, [ebp + 20]  ; head
    mov al, [ebp + 24]  ; count

    mov ebx, [ebp + 28] ; dataOut
    
    ; Convert linear address to seg:off
    mov esi, ebx
    shr esi, 4
    and ebx, 0xF

    mov [g_RegDX], dx
    mov [g_RegCX], cx
    mov [g_RegAX], ax
    mov [g_RegES], si
    mov [g_RegBX], bx

    call x86_EnterRealMode
    
    [bits 16]
    a32 mov dx, [g_RegDX]
    a32 mov cx, [g_RegCX]
    a32 mov ax, [g_RegAX]
    a32 mov si, [g_RegES]
    mov es, si
    a32 mov bx, [g_RegBX]
    
    mov ah, 0x02
    stc
    int 0x13
    
    pushf
    pop ax
    a32 mov [g_RegFlags], ax
    
    call x86_EnterProtectedMode
    [bits 32]

    ; Check carry flag (bit 0 = error)
    movzx eax, word [g_RegFlags]
    test al, 1
    jnz .error
    mov eax, 1
    jmp .done
.error:
    xor eax, eax
.done:

    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret

; ------------------------------------------------------------------------------
; x86_Disk_GetDriveParams
; bool x86_Disk_GetDriveParams(uint8_t drive, uint8_t* driveTypeOut, uint16_t* cylindersOut, uint16_t* sectorsOut, uint16_t* headsOut);
; ------------------------------------------------------------------------------
x86_Disk_GetDriveParams:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov dl, [ebp + 8]
    mov ah, 0x08
    
    mov [g_RegDX], dx
    mov [g_RegAX], ax
    
    call x86_EnterRealMode
    
    [bits 16]
    a32 mov dx, [g_RegDX]
    a32 mov ax, [g_RegAX]
    xor di, di
    mov es, di
    stc
    int 0x13
    
    pushf
    pop ax
    a32 mov [g_RegFlags], ax
    a32 mov [g_RegCX], cx
    a32 mov [g_RegDX], dx
    a32 mov [g_RegBX], bx
    
    call x86_EnterProtectedMode
    [bits 32]

    mov ax, [g_RegFlags]
    and ax, 1
    test ax, ax
    jnz .error

    ; Success, store results
    mov bl, [g_RegBX] ; drive type (BL)
    mov esi, [ebp + 12]
    mov [esi], bl

    mov cx, [g_RegCX]
    mov dx, [g_RegDX]

    ; Cylinders
    mov al, ch
    mov ah, cl
    shr ah, 6
    mov esi, [ebp + 16]
    movzx eax, ax
    inc eax ; Cylinders are 0-based? No, usually returned as max cylinder index?
    ; INT 13h AH=08h: CX = max cylinder/sector.
    ; Cylinders = (CH | ((CL & 0xC0) << 2)) + 1
    mov [esi], ax

    ; Sectors
    mov al, cl
    and al, 0x3F
    mov esi, [ebp + 20]
    movzx eax, al
    mov [esi], ax

    ; Heads
    mov al, dh
    mov esi, [ebp + 24]
    movzx eax, al
    inc eax ; Heads are 0-based count? INT 13h returns max head number (0-based). So count is DH+1.
    mov [esi], ax

    mov eax, 1
    jmp .done

.error:
    xor eax, eax
.done:
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret


; ------------------------------------------------------------------------------
; Mode Switching
; ------------------------------------------------------------------------------

x86_EnterRealMode:
    [bits 32]
    cli
    mov [saved_esp], esp
    
    ; Calculate offset for 16-bit protected mode jump
    mov eax, real16_mode
    sub eax, 0x20000
    
    ; Jump to 16-bit protected mode segment (0x18)
    push dword 0x18                     ; CS
    push eax                            ; EIP
    retf

real16_mode:
    [bits 16]
    ; Setup 16-bit stack for Real Mode transition
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0xFFF0  ; Use top of 64KB segment to avoid overwriting code
    mov fs, ax
    mov gs, ax

    mov eax, cr0
    and eax, ~1
    mov cr0, eax

    ; Jump to Real Mode using retf
    ; Push CS (0x2000)
    push word 0x2000
    
    ; Calculate offset for Real Mode jump
    mov eax, real_mode_entry
    sub eax, 0x20000
    push ax
    retf

real_mode_entry:
    [bits 16]
    ; Disable interrupts just in case
    cli
    
    ; Setup segments for Real Mode
    ; Use DS=0, ES=0 to allow easy access to linear addresses (like g_Reg vars)
    xor ax, ax
    mov ds, ax
    mov es, ax
    
    ; Stack at 0x2FFF0 (SS=0x2000, SP=0xFFF0)
    mov ax, 0x2000
    mov ss, ax
    mov sp, 0xFFF0
    
    ; Restore segments (Wait, we just set them?)
    ; The previous code had "Restore segments" block which set DS=0.
    ; But then it set DS=0x2000 at the top.
    ; Let's clean this up.
    
    mov eax, RealModeIDT
    lidt [eax]
    sti

    ; Fix: Retrieve return address from Protected Mode Stack
    ; saved_esp is at 0x20xxx. DS=0.
    ; We need 32-bit address override.
    a32 mov eax, [saved_esp]
    mov edx, [eax]          ; Get return address (EIP)
    a32 add dword [saved_esp], 4 ; Pop return address from PM stack

    sub edx, 0x20000        ; Convert to offset relative to 0x20000
    
    push word 0x2000        ; CS
    push dx                 ; IP

    retf

x86_EnterProtectedMode:
    [bits 16]
    cli
    
    ; Save Stack Pointer to a known location (0x7C00 is free now)
    mov [0x7C00], sp

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    mov eax, ProtectedModeEntryPtr
    o32 jmp far [eax]

pmode32_entry:
    [bits 32]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    
    ; Get return address from Real Mode stack
    movzx eax, word [0x7C00] ; Get SP
    add eax, 0x20000        ; Linear address of stack
    
    movzx edx, word [eax]   ; Get return address (IP)
    add edx, 0x20000        ; Convert to linear address
    
    mov esp, [saved_esp]
    push edx                ; Push return address onto PM stack
    ret

section .data
saved_esp: dd 0
RealModeIDT: dw 0x3FF
             dd 0

RealModeEntryPtr:
    dd real16_mode - 0x20000
    dw 0x18

RealModeCodePtr:
    dd real_mode_entry - 0x20000
    dw 0x2000

ProtectedModeEntryPtr:
    dd pmode32_entry
    dw 0x08

g_RegAX: dw 0
g_RegBX: dw 0
g_RegCX: dw 0
g_RegDX: dw 0
g_RegES: dw 0
g_RegFlags: dw 0
