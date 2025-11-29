org 0x7C00
bits 16


%define ENDL 0x0D, 0x0A


;
;   FAT12 header
;
jmp short start
nop

bdb_oem:                    db 'MSWIN4.1'           ; 8 bytes
bdb_bytes_per_sector:       dw 512                  ; bytes per sector
bdb_sectors_per_cluster:    db 1                    ; sectors per cluster
bdb_reserved_sectors:       dw 1                    ; reserved sectors
bdb_fat_count:              db 2                    ; number of FATs
bdb_dir_entries_count:      dw 0E0h                 ; max number of root dir entries
bdb_total_sectors:          dw 2880                 ; total sectors (for disks < 32MB)
bdb_media_descriptor_type:  db 0F0h                 ; media descriptor, indicates F0 = 3.5" floppy disk
bdb_sectors_per_fat:        dw 9                    ; sectors per FAT
bdb_sectors_per_track:      dw 18                   ; sectors per track
bdb_heads:                  dw 2                    ; number of heads
bdb_hidden_sectors:         dd 0                    ; hidden sectors
bdb_large_sector_count:     dd 0                    ; total sectors (for disks >=


; extended boot record 
ebr_drive_number:          db 0                     ; drive number (0x00 = floppy, 0x80 = hdd)
                           db 0                     ; reserved
ebr_signature:             db 29h                   ; extended boot signature
ebr_volume_id:             dd 12h, 34h, 56h, 78h    ; volume id
ebr_volume_label:          db 'NANOBYTE OS'         ; volume label
ebr_system_id:             db 'FAT12   '            ; file system type, 8 bytes


start:
    ;   setup data segments
    mov ax, 0         ;  can't write to ds/es directely
    mov ds, ax
    mov es, ax   
    
    ; setup stack
    mov ss, ax
    mov sp, 0x7C00   ; stack grows down from where we are loaded in memory

    push es
    push word .after
    retf
.after:


    ; let's try to read some sectors from the disk after writing all this code, shall we? ( ofc we shall ;) )
    mov [ebr_drive_number], dl

    ; print message
    mov si, msg_loading
    call puts

    push es
    mov ah, 08h
    int 13h
    jc floppy_error
    pop es

    and cl, 0x3F
    xor ch, ch
    mov [bdb_sectors_per_track], cx

    inc dh
    mov [bdb_heads], dh

    mov ax, [bdb_sectors_per_fat]
    mov bl, [bdb_fat_count]
    xor bh, bh
    mul bx
    add ax, [bdb_reserved_sectors]
    push ax

    mov ax, [bdb_sectors_per_fat]
    shl ax, 5
    xor dx, dx
    div word [bdb_bytes_per_sector]

    test dx, dx
    jz .root_dir_after
    inc ax

.root_dir_after:
    mov cl, al
    pop ax
    mov dl, [ebr_drive_number]
    mov bx, buffer
    call disk_read

    ; Compute data_start_lba = reserved + (fat_count * sectors_per_fat) + root_dir_sectors
    mov ax, [bdb_dir_entries_count]
    shl ax, 5                   ; *32
    add ax, 511
    shr ax, 9                   ; /512 = root_dir_sectors
    mov cx, ax
    mov ax, [bdb_sectors_per_fat]
    mov bl, [bdb_fat_count]
    xor bh, bh
    mul bx
    add ax, [bdb_reserved_sectors]
    add ax, cx
    mov [data_start_lba], ax

    xor bx, bx
    mov di, buffer

.search_stage2:
    mov si, file_stage2_bin
    mov cx, 11
    push di
    repe cmpsb
    pop di
    je .found_stage2

    add di, 32
    inc bx
    cmp bx, [bdb_dir_entries_count]
    jl .search_stage2

    jmp stage2_not_found_error

.found_stage2:

    mov ax, [di + 26]
    mov [stage2_cluster], ax

    mov ax, [bdb_reserved_sectors]
    mov bx, buffer
    mov cl, [bdb_sectors_per_fat]
    mov dl, [ebr_drive_number]
    call disk_read

    mov bx, stage2_LOAD_SEGMENT
    mov es, bx
    mov bx, stage2_LOAD_OFFSET

.load_stage2_loop:
    ; LBA = data_start_lba + (cluster - 2)
    mov ax, [stage2_cluster]
    sub ax, 2
    add ax, [data_start_lba]

    mov cl, 1
    mov dl, [ebr_drive_number]
    call disk_read

    add bx, [bdb_bytes_per_sector]          ; if stage2.bin is > 64KB, data corruption will occour... <-------------------------------------------------------------- agent, fix this as well pls! :D 

    mov ax, [stage2_cluster]
    mov cx, 3
    mul cx
    mov cx, 2
    div cx

    mov si, buffer
    add si, ax
    mov ax, [ds:si]

    or dx, dx
    jz .even

.odd:
    shr ax, 4
    jmp .next_cluster_after

.even:
    and ax, 0x0FFF

.next_cluster_after:
    cmp ax, 0x0FF8
    jae .read_finish

    mov [stage2_cluster], ax
    jmp .load_stage2_loop

.read_finish:
    mov dl, [ebr_drive_number]
    mov ax, stage2_LOAD_SEGMENT
    mov ds, ax
    mov es, ax

    jmp stage2_LOAD_SEGMENT:stage2_LOAD_OFFSET

;
;   Error handling
;

floppy_error:
    mov si, msg_read_failed
    call puts
    jmp wait_key_and_reboot

stage2_not_found_error:
    mov si, msg_stage2_not_found
    call puts
    jmp wait_key_and_reboot

wait_key_and_reboot:
    mov ah, 0
    int 16h
    jmp 0FFFFh:0

;
;   Disk routines
;

;
;   Converts LBA address to CHS
;   Parameters:
;   - ax: LBA address
;   Returns:
;   - cx [bits 0-5]: sector number
;   - cx [bits 6-15]: cylinder
;   - dh: head

lba_to_chs:

    push ax
    push dx

    xor dx, dx                              ; dx = 0
    div word [bdb_sectors_per_track]        ; ax = LBA / sectors per track, dx = LBA % sectors per track

    inc dx                                  ; dx = (LBA % sectors per track) + 1  = sector
    mov cx, dx                              ; cx = sector

    xor dx, dx                              ; dx = 0
    div word [bdb_heads]                    ; ax = (LBA / sectors per track) / heads = cylinder
                                            ; dx = (LBA / sectors per track) % heads = head
    mov dh, dl                              ; dh = head
    mov ch, al                              ; ch = cylinder (lower 8 bits)
    shl ah, 6
    or cl, ah                               ; put upper 2 bits of cylinder in cl

    pop ax
    mov dl, al                              ; restore dl (drive number)
    pop ax
    ret

;
;   Reads sectors from a disk
;   Parameters:
;   - ax: LBA address
;   - cl: number of sectors to read (up to 128)
;   - dl: drive number
;   - es:bx: memory address where to store read data

disk_read:
    push ax
    push bx
    push cx
    push dx
    push di

    push cx                     ; Save sector count
    call lba_to_chs
    pop ax                      ; Restore as sector count in AL

    mov ah, 0x02
    mov di, 3

.retry:
    pusha                       ; save all registers, we don't know what BIOS will modify
    stc                         ; set carry flag before calling BIOS because some BIOS'es won't set it ;)
    int 13h                     ; call BIOS disk interrupt, carry flag cleared = success
    jnc .done                   ; if no error, done. this works because we set carry flag before calling BIOS :)

    ; if read failed
    popa                        ; restore all registers, again, we don't know what BIOS will modify. AI auto comment recommendations are getting preety handy :) (the ":)" was recommended by AI as well XD and so was the "XD")
    call disk_reset             ; reset disk system

    dec di                      ; decrement retry count
    test di, di                 ; check if we have retries left
    jnz .retry                  ; if we do, try again


.fail:
    jmp floppy_error

.done:
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret


disk_reset:
    pusha
    mov ah, 0                 ; BIOS reset disk system function
    stc
    int 13h                     ; call BIOS disk interrupt
    jc floppy_error
    popa
    ret

;
; Prints a string to the screen
; Params:
;   - ds:si points to string
;
puts:
    ; save register we will modify
    push si
    push ax

.loop:
    lodsb           ; loads next character in al
    or al, al       ; verify if next character is null?
    jz .done

    mov ah, 0x0e    ; BIOS teletype function
    mov bh, 0       ; page number
    int 0x10        ; call BIOS video interrupt
    jmp .loop

.done:
    pop ax
    pop si
    ret

msg_loading:            db 'Load', ENDL, 0
msg_read_failed:        db 'Disk!', ENDL, 0
msg_stage2_not_found:   db 'STAGE2!', ENDL, 0
file_stage2_bin:        db 'STAGE2  BIN'
stage2_cluster:         dw 0
data_start_lba:         dw 0

stage2_LOAD_SEGMENT     equ 0x2000
stage2_LOAD_OFFSET      equ 0

times 510-($-$$) db 0
dw 0AA55h

buffer:

