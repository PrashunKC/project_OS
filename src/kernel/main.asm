org 0x0
bits 16


%define ENDL 0x0D, 0x0A


start:
    ; Set up segment registers for stage2
    mov ax, 0x2000
    mov ds, ax
    mov es, ax
    
    mov si, msg_hello
    call puts

.halt:
    cli
    hlt

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
    mov bh, 0       ; page number, he forgot this in his video :)
    int 0x10        ; call BIOS video interrupt
    jmp .loop       ; he forgot this in his video :)

.done:
    pop ax
    pop si
    ret


msg_hello: db 'Hello world from stage2!', ENDL, 0