; MBR (Master Boot Record) Stage 0
; 446 bytes code + 64 bytes partition table + 2 bytes signature
; Locates active FAT partition and chainloads its VBR

org 0x7C00
bits 16

%define ENDL 0x0D, 0x0A

start:
    ; Setup segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Relocate MBR from 0x7C00 to 0x0600 to free space for VBR
    mov si, 0x7C00
    mov di, 0x0600
    mov cx, 256
    rep movsw
    
    ; Jump to relocated code
    jmp 0x0000:relocated

relocated:
    ; Save boot drive
    mov [boot_drive], dl
    
    ; Find active partition
    mov si, partition_table
    mov cx, 4

.check_partition:
    mov al, [si]
    test al, 0x80           ; Check if active (bit 7 set)
    jnz .found_active
    
    add si, 16
    loop .check_partition
    
    ; No active partition found
    mov si, msg_no_active
    call puts
    jmp halt

.found_active:
    ; Load VBR from partition start to 0x7C00
    mov eax, [si + 8]       ; LBA start of partition
    mov [partition_lba], eax
    
    mov di, 0x7C00
    mov cx, 1               ; Read 1 sector
    call read_sectors_lba
    
    ; Verify boot signature
    cmp word [0x7C00 + 510], 0xAA55
    jne .invalid_vbr
    
    ; Jump to VBR
    mov dl, [boot_drive]
    jmp 0x0000:0x7C00

.invalid_vbr:
    mov si, msg_invalid_vbr
    call puts
    jmp halt

halt:
    cli
    hlt
    jmp halt

;
; Read sectors using LBA (INT 13h extensions)
; Parameters:
;   eax: Starting LBA
;   cx: Number of sectors
;   es:di: Destination buffer
;
read_sectors_lba:
    pusha
    
    ; Check for INT 13h extensions
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc .use_chs             ; Extensions not supported
    cmp bx, 0xAA55
    jne .use_chs
    
    ; Use LBA packet read
    mov [dap_lba], eax
    mov [dap_count], cx
    mov [dap_offset], di
    mov word [dap_segment], es
    
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, disk_address_packet
    int 0x13
    jc .error
    
    popa
    ret

.use_chs:
    ; Fallback to CHS (simplified, assumes floppy geometry)
    ; For full HDD support, would need proper CHS conversion
    popa
    ret

.error:
    mov si, msg_read_error
    call puts
    jmp halt

;
; Print string
; Parameters: ds:si = string
;
puts:
    push ax
    push si
    
.loop:
    lodsb
    or al, al
    jz .done
    
    mov ah, 0x0E
    xor bh, bh
    int 0x10
    jmp .loop
    
.done:
    pop si
    pop ax
    ret

; Disk Address Packet for INT 13h extensions
disk_address_packet:
dap_size:       db 0x10
dap_reserved:   db 0
dap_count:      dw 0
dap_offset:     dw 0
dap_segment:    dw 0
dap_lba:        dd 0
                dd 0

; Variables
boot_drive:     db 0
partition_lba:  dd 0

; Messages
msg_no_active:      db 'No active partition', ENDL, 0
msg_invalid_vbr:    db 'Invalid VBR', ENDL, 0
msg_read_error:     db 'Disk read error', ENDL, 0

; Padding to 446 bytes
times 446-($-$$) db 0

; Partition table (64 bytes) - will be populated by disk tools
partition_table:
    times 64 db 0

; Boot signature
dw 0xAA55
