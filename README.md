# project_OS - A Custom Operating System from Scratch

My journey of scouring the depths of the internet to learn and build my very own OS (hopefully)

## Table of Contents
1. [Project Overview](#project-overview)
2. [System Architecture](#system-architecture)
3. [Detailed Component Breakdown](#detailed-component-breakdown)
   - [Bootloader (boot.asm)](#bootloader-bootasm)
   - [Kernel (main.asm)](#kernel-mainasm)
   - [FAT12 Utility (fat.c)](#fat12-utility-fatc)
   - [Build System (Makefile)](#build-system-makefile)
4. [How Everything Works Together](#how-everything-works-together)
5. [Building and Running](#building-and-running)
6. [Technical Details](#technical-details)

---

## Project Overview

This is a custom operating system built from scratch that demonstrates the fundamental concepts of OS development. The project includes:

- **A custom bootloader** written in x86 assembly that loads the OS kernel from a FAT12 filesystem
- **A simple kernel** that displays a message when loaded
- **A FAT12 filesystem utility** for reading files from the disk image
- **A complete build system** that creates a bootable floppy disk image

The OS is designed to run on x86 architecture (16-bit real mode) and can be executed in emulators like QEMU or on real hardware.

---

## System Architecture

### Boot Process Flow

```
Power On → BIOS → Bootloader (512 bytes) → Kernel → OS Running
```

1. **BIOS Stage**: When the computer starts, the BIOS loads the first sector (512 bytes) of the boot disk into memory at address `0x7C00` and executes it
2. **Bootloader Stage**: The bootloader initializes hardware, reads the FAT12 filesystem, locates the kernel file, loads it into memory, and transfers control to it
3. **Kernel Stage**: The kernel takes over and runs the operating system

### Memory Layout

```
0x0000:0x0000 - Interrupt Vector Table
0x0000:0x7C00 - Bootloader loaded here (512 bytes)
0x0000:0x7E00 - Buffer for disk operations
0x2000:0x0000 - Kernel loaded here
```

---

## Detailed Component Breakdown

### Bootloader (boot.asm)

The bootloader is the first piece of code that runs when the system starts. It's limited to exactly 512 bytes (one sector) and must end with the boot signature `0xAA55`.

#### **1. Initial Setup and FAT12 Header (Lines 1-36)**

```assembly
org 0x7C00
bits 16
```

- `org 0x7C00`: Tells the assembler that this code will be loaded at memory address 0x7C00 by the BIOS
- `bits 16`: Specifies that we're using 16-bit real mode instructions

**FAT12 Filesystem Header (Lines 8-35):**
The bootloader pretends to be a FAT12 filesystem by including a BIOS Parameter Block (BPB). This is crucial because:
- The disk is formatted as FAT12
- The BPB contains vital information about the filesystem structure
- Modern operating systems expect this structure

Key fields:
- `bdb_bytes_per_sector: 512`: Each sector is 512 bytes
- `bdb_sectors_per_cluster: 1`: Each cluster is 1 sector
- `bdb_reserved_sectors: 1`: The bootloader itself occupies 1 reserved sector
- `bdb_fat_count: 2`: Two copies of the File Allocation Table (for redundancy)
- `bdb_dir_entries_count: 0E0h (224)`: Maximum 224 files in the root directory
- `bdb_total_sectors: 2880`: Total disk size = 2880 sectors = 1.44MB (standard floppy)
- `bdb_sectors_per_fat: 9`: Each FAT occupies 9 sectors

**Extended Boot Record (Lines 30-35):**
- `ebr_drive_number`: Will store which drive we booted from (0x00 = floppy, 0x80 = hard disk)
- `ebr_volume_label: 'NANOBYTE OS'`: The volume name
- `ebr_system_id: 'FAT12   '`: Identifies this as FAT12

#### **2. Segment Register Initialization (Lines 38-51)**

```assembly
start:
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
```

**Why this is necessary:**
- In real mode, memory addresses are calculated as: `segment * 16 + offset`
- We set all segment registers (ds, es, ss) to 0 to use simple linear addressing
- Cannot write directly to segment registers, must go through a general-purpose register (ax)
- Stack pointer (sp) is set to 0x7C00 (where bootloader is) because the stack grows downward

```assembly
push es
push word .after
retf
```

This clever trick performs a "far return" to ensure CS (code segment) is also set to 0. It:
1. Pushes the ES register (which is 0) onto the stack
2. Pushes the address of `.after` label
3. Performs a far return which pops both values: CS gets ES (0), IP gets .after address

#### **3. Disk Geometry Detection (Lines 54-72)**

```assembly
mov [ebr_drive_number], dl
```
The BIOS passes the boot drive number in DL register - we save it for later use.

```assembly
push es
mov ah, 08h
int 13h
jc floppy_error
pop es
```

**BIOS INT 13h, AH=08h** - Get drive parameters:
- Input: DL = drive number
- Output: CL (bits 0-5) = sectors per track, DH = maximum head number
- This detects the actual disk geometry instead of assuming fixed values

```assembly
and cl, 0x3F          ; Extract sectors per track (bits 0-5 of CL)
xor ch, ch            ; Clear CH
mov [bdb_sectors_per_track], cx

inc dh                ; DH had max head number, add 1 to get total heads
mov [bdb_heads], dh
```

**Why detect geometry?** Different floppy types have different geometries. This makes the bootloader more flexible.

#### **4. Calculate Root Directory Location (Lines 74-95)**

The FAT12 filesystem layout is:
```
[Boot Sector][FAT 1][FAT 2][Root Directory][Data Area]
```

To find the root directory, we calculate:

```assembly
mov ax, [bdb_sectors_per_fat]    ; AX = 9 (sectors per FAT)
mov bl, [bdb_fat_count]          ; BL = 2 (number of FATs)
xor bh, bh                        ; BH = 0
mul bx                            ; AX = 9 * 2 = 18 (total FAT sectors)
add ax, [bdb_reserved_sectors]   ; AX = 18 + 1 = 19
push ax                           ; Save this for later
```

Result: Root directory starts at sector 19

**Calculate root directory size:**

```assembly
mov ax, [bdb_sectors_per_fat]    ; AX = 9
shl ax, 5                         ; AX = 9 * 32 = 288 (bytes in root dir)
xor dx, dx
div word [bdb_bytes_per_sector]  ; AX = 288 / 512 = 0, DX = 288

test dx, dx                       ; Check remainder
jz .root_dir_after
inc ax                            ; If remainder, need one more sector
```

Calculation: 224 entries * 32 bytes/entry = 7168 bytes = 14 sectors

#### **5. Read Root Directory (Lines 91-95)**

```assembly
.root_dir_after:
    mov cl, al                    ; CL = number of sectors to read
    pop ax                        ; AX = LBA of root directory (19)
    mov dl, [ebr_drive_number]   ; DL = drive number
    mov bx, buffer               ; ES:BX = where to store data
    call disk_read               ; Read sectors
```

This reads the root directory into memory starting at the `buffer` label.

#### **6. Search for Kernel File (Lines 98-114)**

```assembly
xor bx, bx              ; BX = 0 (entry counter)
mov di, buffer          ; DI points to first directory entry

.search_kernel:
    mov si, file_kernel_bin    ; SI points to "KERNEL  BIN"
    mov cx, 11                  ; 11 characters to compare
    push di                     ; Save DI
    repe cmpsb                  ; Compare strings
    pop di                      ; Restore DI
    je .found_kernel           ; If equal, we found it!

    add di, 32                  ; Move to next entry (32 bytes each)
    inc bx                      ; Increment counter
    cmp bx, [bdb_dir_entries_count]  ; Checked all entries?
    jl .search_kernel          ; If not, continue

    jmp kernel_not_found_error  ; Kernel not found!
```

**Directory Entry Structure:** Each entry is 32 bytes:
- Bytes 0-10: Filename (8.3 format, padded with spaces)
- Bytes 11-25: Various attributes and timestamps
- Bytes 26-27: First cluster number
- Bytes 28-31: File size

**String Comparison:** `repe cmpsb` compares bytes at DS:SI with ES:DI, repeating CX times while they're equal.

#### **7. Load File Allocation Table (Lines 116-125)**

```assembly
.found_kernel:
    mov ax, [di + 26]           ; Get first cluster number (offset 26 in directory entry)
    mov [kernel_cluster], ax    ; Save it

    mov ax, [bdb_reserved_sectors]  ; AX = 1 (FAT starts after boot sector)
    mov bx, buffer
    mov cl, [bdb_sectors_per_fat]   ; CL = 9
    mov dl, [ebr_drive_number]
    call disk_read                   ; Read FAT into buffer
```

The FAT is a table that tells us which clusters belong to our file and in what order.

#### **8. Load Kernel into Memory (Lines 127-174)**

```assembly
mov bx, KERNEL_LOAD_SEGMENT      ; BX = 0x2000
mov es, bx                        ; ES = 0x2000
mov bx, KERNEL_LOAD_OFFSET       ; BX = 0
```

Sets destination address to 0x2000:0x0000 (absolute address 0x20000)

```assembly
.load_kernel_loop:
    mov ax, [kernel_cluster]
    add ax, 31                    ; Convert cluster to LBA
```

**Cluster to LBA conversion:**
- Cluster 2 is the first data cluster
- Data area starts after: boot sector (1) + FATs (18) + root dir (14) = sector 33
- Formula: LBA = 33 + (cluster - 2) = cluster + 31

```assembly
    mov cl, 1
    mov dl, [ebr_drive_number]
    call disk_read                ; Read one sector

    add bx, [bdb_bytes_per_sector]  ; Move buffer pointer forward
```

Reads the cluster and advances the buffer pointer. Note: If kernel > 64KB, this would cause segment overflow (mentioned in code comment).

**Navigate FAT12 Chain (Lines 141-166):**

FAT12 uses 12 bits per entry, so entries are packed:
```
Byte 0: [Entry 0 low 8 bits]
Byte 1: [Entry 1 low 4 bits][Entry 0 high 4 bits]
Byte 2: [Entry 1 high 8 bits]
```

```assembly
    mov ax, [kernel_cluster]
    mov cx, 3
    mul cx                        ; AX = cluster * 3
    mov cx, 2
    div cx                        ; AX = cluster * 3 / 2, DX = remainder
```

This calculates the byte offset in the FAT.

```assembly
    mov si, buffer
    add si, ax                    ; SI points to FAT entry
    mov ax, [ds:si]              ; Load 16 bits

    or dx, dx                     ; Check if cluster is even or odd
    jz .even

.odd:
    shr ax, 4                     ; Shift right 4 bits
    jmp .next_cluster_after

.even:
    and ax, 0x0FFF               ; Mask to 12 bits
```

Extracts the 12-bit cluster number based on whether cluster is even or odd.

```assembly
.next_cluster_after:
    cmp ax, 0x0FF8               ; Check for end-of-file marker
    jae .read_finish             ; If >= 0xFF8, file is complete

    mov [kernel_cluster], ax     ; Save next cluster
    jmp .load_kernel_loop        ; Load next cluster
```

Values 0xFF8-0xFFF indicate end of file in FAT12.

#### **9. Jump to Kernel (Lines 168-174)**

```assembly
.read_finish:
    mov dl, [ebr_drive_number]   ; Pass drive number to kernel
    mov ax, KERNEL_LOAD_SEGMENT
    mov ds, ax
    mov es, ax

    jmp KERNEL_LOAD_SEGMENT:KERNEL_LOAD_OFFSET
```

Sets segment registers and performs a far jump to the kernel at 0x2000:0x0000.

#### **10. Error Handling (Lines 176-193)**

```assembly
floppy_error:
    mov si, msg_read_failed
    call puts
    jmp wait_key_and_reboot

kernel_not_found_error:
    mov si, msg_kernel_not_found
    call puts
    jmp wait_key_and_reboot

wait_key_and_reboot:
    mov ah, 0                     ; BIOS keyboard function: wait for key
    int 16h                       ; Wait for keypress
    jmp 0FFFFh:0                 ; Jump to BIOS reset vector
```

If anything fails, display an error message, wait for a key, and reboot.

#### **11. Disk Routines (Lines 195-294)**

**LBA to CHS Conversion (Lines 208-230):**

Modern code uses LBA (Logical Block Addressing), but BIOS INT 13h requires CHS (Cylinder, Head, Sector).

```assembly
lba_to_chs:
    xor dx, dx
    div word [bdb_sectors_per_track]
    ; AX = LBA / sectors_per_track (track number)
    ; DX = LBA % sectors_per_track

    inc dx                        ; Sectors start at 1, not 0
    mov cx, dx                    ; CX = sector

    xor dx, dx
    div word [bdb_heads]
    ; AX = track / heads = cylinder
    ; DX = track % heads = head

    mov dh, dl                    ; DH = head
    mov ch, al                    ; CH = cylinder (low 8 bits)
    shl ah, 6
    or cl, ah                     ; CL bits 6-7 = cylinder high 2 bits
```

**Disk Read Function (Lines 240-284):**

```assembly
disk_read:
    push ax
    push bx
    push cx
    push dx
    push di                       ; Save all registers

    push cx                       ; Save sector count
    call lba_to_chs              ; Convert LBA to CHS
    pop ax                        ; AL = sector count

    mov ah, 02h                   ; BIOS function: read sectors
    mov di, 3                     ; Retry count (floppies are unreliable!)

.retry:
    pusha                         ; Save all registers
    stc                           ; Set carry flag (some BIOS don't set it)
    int 13h                       ; BIOS disk read
    jnc .done                     ; If no error (carry clear), done

    popa                          ; Restore registers
    call disk_reset               ; Reset disk system
    dec di                        ; Decrement retry count
    test di, di
    jnz .retry                    ; Retry if count > 0

.fail:
    jmp floppy_error              ; All retries failed

.done:
    popa                          ; Restore registers
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret
```

**Why retry?** Floppy disks are mechanical and can fail to read on first attempt. Three retries is standard practice.

**Disk Reset Function (Lines 287-294):**

```assembly
disk_reset:
    pusha
    mov ah, 0                     ; BIOS function: reset disk system
    stc
    int 13h
    jc floppy_error
    popa
    ret
```

#### **12. Screen Output Function (Lines 296-319)**

```assembly
puts:
    push si
    push ax

.loop:
    lodsb                         ; Load byte at DS:SI into AL, increment SI
    or al, al                     ; Check if AL is 0 (null terminator)
    jz .done

    mov ah, 0x0e                  ; BIOS teletype function
    mov bh, 0                     ; Page number
    int 0x10                      ; BIOS video interrupt
    jmp .loop

.done:
    pop ax
    pop si
    ret
```

**BIOS INT 10h, AH=0Eh:** Teletype output - prints character in AL to screen and advances cursor.

#### **13. Data Section (Lines 321-334)**

```assembly
msg_loading:            db 'Loading...', ENDL, 0
msg_read_failed:        db 'Read from disk failed!', ENDL, 0
msg_kernel_not_found:   db 'KERNEL.BIN not found!', ENDL, 0
file_kernel_bin:        db 'KERNEL  BIN'      ; 11 bytes, space-padded
kernel_cluster:         dw 0

KERNEL_LOAD_SEGMENT     equ 0x2000
KERNEL_LOAD_OFFSET      equ 0

times 510-($-$$) db 0                         ; Pad to 510 bytes
dw 0AA55h                                     ; Boot signature

buffer:                                        ; Buffer starts at offset 512
```

**Boot Signature:** The last 2 bytes must be 0xAA55 or BIOS won't recognize it as bootable.

**`times 510-($-$$) db 0`:**
- `$` = current position
- `$$` = start of section
- `$-$$` = bytes written so far
- `510-($-$$)` = remaining bytes to reach 510
- Pads with zeros to exactly 510 bytes, leaving room for the 2-byte signature

---

### Kernel (main.asm)

The kernel is much simpler than the bootloader. It's loaded at 0x2000:0x0000 and simply displays a message.

#### **1. Initial Setup (Lines 1-12)**

```assembly
org 0x0
bits 16
```

- `org 0x0`: Code assumes offset 0 (because we'll set segment to 0x2000)
- `bits 16`: Still in 16-bit real mode

```assembly
start:
    mov ax, 0x2000                ; Segment where kernel is loaded
    mov ds, ax                    ; Set data segment
    mov es, ax                    ; Set extra segment
```

Sets up segments to point to where the kernel is in memory.

#### **2. Display Message (Lines 14-15)**

```assembly
    mov si, msg_hello             ; SI points to message string
    call puts                     ; Print it
```

Uses the same `puts` function pattern as the bootloader.

#### **3. Halt System (Lines 17-19)**

```assembly
.halt:
    cli                           ; Clear interrupts (disable them)
    hlt                           ; Halt the CPU until next interrupt
```

**Why this loop?**
- `hlt` saves power by stopping CPU execution
- But interrupts can wake it up (even with `cli`, NMI can still occur)
- The loop ensures we halt again if woken up

#### **4. Screen Output Function (Lines 21-44)**

Identical to bootloader's `puts` function:

```assembly
puts:
    push si
    push ax

.loop:
    lodsb                         ; Load next character
    or al, al                     ; Check for null terminator
    jz .done

    mov ah, 0x0e                  ; BIOS teletype
    mov bh, 0                     ; Page number (forgotten in tutorial video!)
    int 0x10                      ; Print character
    jmp .loop                     ; Continue with next character

.done:
    pop ax
    pop si
    ret
```

#### **5. Data (Line 47)**

```assembly
msg_hello: db 'Hello world from kernel!', ENDL, 0
```

The message that will be displayed when the kernel runs.

**Note:** The code comments mention "he forgot this in his video" - referring to following an online tutorial where some details were missed initially.

---

### FAT12 Utility (fat.c)

This is a command-line utility written in C that can read files from the FAT12 disk image. It's useful for debugging and extracting files without booting the OS.

#### **1. Data Structures (Lines 8-51)**

**BootSector Structure (Lines 8-35):**

```c
typedef struct {
    uint8_t BootJumpInstruction[3];      // First 3 bytes (jmp + nop)
    uint8_t OemIdentifier[8];            // "MSWIN4.1"
    uint16_t BytesPerSector;             // Usually 512
    uint8_t SectorsPerCluster;           // Usually 1 for floppies
    uint16_t ReservedSectors;            // Boot sector (1)
    uint8_t FatCount;                    // Number of FATs (2)
    uint16_t DirEntryCount;              // Root dir entries (224)
    uint16_t TotalSectors;               // Total sectors (2880)
    uint8_t MediaDescriptorType;         // 0xF0 for 3.5" floppy
    uint16_t SectorsPerFat;              // Sectors per FAT (9)
    uint16_t SectorsPerTrack;            // Sectors per track (18)
    uint16_t Heads;                      // Number of heads (2)
    uint32_t HiddenSectors;              // Hidden sectors (0)
    uint32_t LargeSectorCount;           // For disks >= 32MB
    
    uint8_t DriveNumber;                 // 0x00 or 0x80
    uint8_t _Reserved;
    uint8_t Signature;                   // 0x29
    uint8_t VolumeLabel[11];             // Volume name
    uint8_t SystemId[8];                 // "FAT12   "
} __attribute__((packed)) BootSector;
```

**`__attribute__((packed))`:** Tells GCC not to add padding between struct members, ensuring it matches the exact disk layout.

**DirectoryEntry Structure (Lines 37-51):**

```c
typedef struct {
    uint8_t Name[11];                    // 8.3 filename (padded)
    uint8_t Attributes;                  // File attributes
    uint8_t _Reserved;
    uint8_t CreatedTimeTenths;           // Creation time (10ms units)
    uint16_t CreatedTime;                // Creation time (2-sec units)
    uint16_t CreatedDate;                // Creation date
    uint16_t AccessedDate;               // Last access date
    uint16_t FirstClusterHigh;           // High word of first cluster (0 for FAT12)
    uint16_t ModifiedTime;               // Last modified time
    uint16_t ModifiedDate;               // Last modified date
    uint16_t FirstClusterLow;            // Low word of first cluster
    uint32_t Size;                       // File size in bytes
} __attribute__((packed)) DirectoryEntry;
```

#### **2. Global Variables (Lines 53-56)**

```c
BootSector g_BootSector;                 // Stores boot sector
uint8_t* g_Fat = NULL;                   // Pointer to FAT
DirectoryEntry* g_RootDirectory = NULL;  // Pointer to root directory
uint32_t g_RootDirectoryEnd;             // LBA where data area starts
```

#### **3. Reading Boot Sector (Lines 58-61)**

```c
bool readBootSector(FILE* disk) {
    return fread(&g_BootSector, sizeof(BootSector), 1, disk) > 0;
}
```

Simple: read the first 512 bytes into the BootSector struct.

#### **4. Reading Sectors (Lines 63-69)**

```c
bool readSectors(FILE* disk, uint32_t lba, uint32_t count, void* bufferOut) {
    bool ok = true;
    ok = ok && (fseek(disk, lba * g_BootSector.BytesPerSector, SEEK_SET) == 0);
    ok = ok && (fread(bufferOut, g_BootSector.BytesPerSector, count, disk) == count);
    return ok;
}
```

Generic function to read sectors from disk:
1. Seek to `lba * 512` bytes from start
2. Read `count` sectors
3. Return true if both operations succeeded

#### **5. Reading FAT (Lines 71-75)**

```c
bool readFat(FILE* disk) {
    g_Fat = (uint8_t*) malloc(g_BootSector.SectorsPerFat * g_BootSector.BytesPerSector);
    return readSectors(disk, g_BootSector.ReservedSectors, g_BootSector.SectorsPerFat, g_Fat);
}
```

Allocates memory and reads the FAT:
- FAT starts at sector 1 (after boot sector)
- FAT is 9 sectors = 4608 bytes

#### **6. Reading Root Directory (Lines 77-88)**

```c
bool readRootDirectory(FILE* disk) {
    uint32_t lba = g_BootSector.ReservedSectors + 
                   (g_BootSector.FatCount * g_BootSector.SectorsPerFat);
    uint32_t size = g_BootSector.DirEntryCount * sizeof(DirectoryEntry);
    uint32_t sectors = (size / g_BootSector.BytesPerSector);
    
    if(size % g_BootSector.BytesPerSector > 0) sectors++;
    
    g_RootDirectoryEnd = lba + sectors;
    g_RootDirectory = (DirectoryEntry*) malloc(sectors * g_BootSector.BytesPerSector);
    return readSectors(disk, lba, sectors, g_RootDirectory);
}
```

Calculates root directory location:
- LBA = 1 (boot) + 18 (two FATs) = 19
- Size = 224 entries * 32 bytes = 7168 bytes = 14 sectors
- Also calculates `g_RootDirectoryEnd` (sector 33) for later use

#### **7. Finding a File (Lines 90-101)**

```c
DirectoryEntry* findFile(const char* name) {
    for(uint32_t i = 0; i < g_BootSector.DirEntryCount; i++) {
        if(memcmp(name, g_RootDirectory[i].Name, 11) == 0) {
            return &g_RootDirectory[i];
        }
    }
    return NULL;
}
```

Linear search through all directory entries, comparing the 11-character filename.

#### **8. Reading a File (Lines 103-126)**

```c
bool readFile(DirectoryEntry* fileEntry, FILE* disk, uint8_t* outputBuffer) {
    bool ok = true;
    uint16_t currentCluster = fileEntry->FirstClusterLow;
    
    do {
        uint32_t lba = g_RootDirectoryEnd + (currentCluster - 2) * g_BootSector.SectorsPerCluster;
        ok = ok && readSectors(disk, lba, g_BootSector.SectorsPerCluster, outputBuffer);
        outputBuffer += g_BootSector.SectorsPerCluster * g_BootSector.BytesPerSector;
        
        uint32_t fatIndex = currentCluster * 3 / 2;
        if(currentCluster % 2 == 0) {
            currentCluster = (*(uint16_t*)(&g_Fat[fatIndex])) & 0x0FFF;
        } else {
            currentCluster = (*(uint16_t*)(&g_Fat[fatIndex])) >> 4;
        }
    } while (ok && currentCluster < 0x0FF8);
    
    return ok;
}
```

**How it works:**
1. **Convert cluster to LBA:** `LBA = g_RootDirectoryEnd + (cluster - 2)`
   - Cluster 2 is the first data cluster
   - Formula accounts for boot sector, FATs, and root directory
2. **Read cluster:** Read the sector(s) into output buffer
3. **Advance buffer:** Move pointer forward by bytes read
4. **Get next cluster from FAT:** Use the FAT12 packing algorithm
   - Even cluster: `entry = (word at fatIndex) & 0x0FFF`
   - Odd cluster: `entry = (word at fatIndex) >> 4`
5. **Repeat until EOF:** Continue until cluster >= 0xFF8

#### **9. Main Function (Lines 128-199)**

```c
int main(int argc, char** argv) {
    if(argc < 3) {
        fprintf(stdout, "Syntax: %s <disk image> <file name>\n", argv[0]);
        return 1;
    }
```

Validates command-line arguments.

```c
    FILE* disk = fopen(argv[1], "rb");
    if(!disk) {
        fprintf(stderr, "Error: Could not open disk image '%s'!\n", argv[1]);
        return -1;
    }
```

Opens the disk image file in binary read mode.

```c
    if(!readBootSector(disk)) { /* error handling */ }
    if(!readFat(disk)) { /* error handling */ }
    if(!readRootDirectory(disk)) { /* error handling */ }
```

Reads the filesystem structures in order.

```c
    DirectoryEntry* fileEntry = findFile(argv[2]);
    if(!fileEntry) {
        fprintf(stderr, "Error: Could not find file '%s' on disk image '%s'!\n", 
                argv[2], argv[1]);
        /* cleanup and return */
    }
```

Searches for the requested file.

```c
    uint8_t* buffer = (uint8_t*) malloc(fileEntry->Size + g_BootSector.BytesPerSector);
    if(!readFile(fileEntry, disk, buffer)) { /* error handling */ }
```

Allocates buffer (with extra space for safety) and reads the entire file.

```c
    for(size_t i = 0; i < fileEntry->Size; i++) {
        if(isprint(buffer[i]) || buffer[i] == '\n' || buffer[i] == '\r' || buffer[i] == '\t') {
            fputc(buffer[i], stdout);
        } else {
            fprintf(stdout, "<%02x>", buffer[i]);
        }
    }
```

Prints the file contents:
- Printable characters → output as-is
- Non-printable → output as hex `<XX>`

```c
    free(buffer);
    free(g_Fat);
    free(g_RootDirectory);
    return 0;
}
```

Cleanup and exit.

---

### Build System (Makefile)

The Makefile orchestrates building all components and creating the bootable disk image.

#### **1. Variables (Lines 1-5)**

```makefile
ASM=nasm                    # Netwide Assembler for x86 assembly
CC=gcc                      # GNU C Compiler
SRC_DIR=src                 # Source code directory
TOOLS_DIR=tools             # Utilities directory
BUILD_DIR=build             # Output directory
```

#### **2. Default Target (Line 10)**

```makefile
all: floppy_image tools_fat
```

Running `make` builds both the bootable floppy image and the FAT utility.

#### **3. Creating Floppy Image (Lines 15-22)**

```makefile
floppy_image: $(BUILD_DIR)/main_floppy.img

$(BUILD_DIR)/main_floppy.img: bootloader kernel
```

Depends on bootloader and kernel being built first.

```makefile
    dd if=/dev/zero of=$(BUILD_DIR)/main_floppy.img bs=512 count=2880
```

**Creates blank 1.44MB floppy image:**
- `if=/dev/zero`: Input is zeros
- `of=...`: Output file
- `bs=512`: Block size 512 bytes
- `count=2880`: 2880 blocks = 1,474,560 bytes = 1.44 MB

```makefile
    mkfs.fat -F 12 -n "NBOS" $(BUILD_DIR)/main_floppy.img
```

**Formats as FAT12 filesystem:**
- `-F 12`: FAT12 filesystem
- `-n "NBOS"`: Volume label "NBOS"

```makefile
    dd if=$(BUILD_DIR)/bootloader.bin of=$(BUILD_DIR)/main_floppy.img conv=notrunc
```

**Writes bootloader to first sector:**
- `conv=notrunc`: Don't truncate output file (preserve FAT12 formatting)
- Overwrites just the first 512 bytes with our bootloader

```makefile
    mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/kernel.bin "::/kernel.bin"
    mcopy -i $(BUILD_DIR)/main_floppy.img test.txt "::/test.txt"
```

**Copies files into FAT12 filesystem:**
- `mcopy`: Part of mtools package, copies files to FAT filesystems
- `-i`: Specifies disk image
- `::/ `: Root directory of the image
- Copies both kernel.bin and test.txt into the filesystem

#### **4. Building Bootloader (Lines 27-30)**

```makefile
bootloader: $(BUILD_DIR)/bootloader.bin

$(BUILD_DIR)/bootloader.bin: always
    $(ASM) $(SRC_DIR)/bootloader/boot.asm -f bin -o $(BUILD_DIR)/bootloader.bin
```

**NASM flags:**
- `-f bin`: Output raw binary (not ELF or other format)
- `-o`: Output file

#### **5. Building Kernel (Lines 35-38)**

```makefile
kernel: $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: always
    $(ASM) $(SRC_DIR)/kernel/main.asm -f bin -o $(BUILD_DIR)/kernel.bin
```

Same process as bootloader - assemble to raw binary.

#### **6. Building FAT Utility (Lines 44-47)**

```makefile
tools_fat: $(BUILD_DIR)/tools/fat

$(BUILD_DIR)/tools/fat: always $(TOOLS_DIR)/fat/fat.c
    mkdir -p $(BUILD_DIR)/tools
    $(CC) -g -o $(BUILD_DIR)/tools/fat $(TOOLS_DIR)/fat/fat.c
```

**GCC flags:**
- `-g`: Include debugging symbols
- `-o`: Output file

Compiles the C utility that can read files from the disk image.

#### **7. Utility Targets (Lines 52-60)**

```makefile
always:
    mkdir -p $(BUILD_DIR)
```

Ensures build directory exists before any build operation.

```makefile
clean:
    rm -rf $(BUILD_DIR)/*
```

Removes all build artifacts.

---

## How Everything Works Together

### Complete Boot Sequence

1. **Power On**
   - BIOS performs POST (Power-On Self Test)
   - BIOS identifies bootable drives

2. **BIOS Loads Bootloader**
   - BIOS reads first sector (512 bytes) from boot disk
   - Loads it to memory address 0x7C00
   - Verifies boot signature (0xAA55)
   - Jumps to 0x7C00

3. **Bootloader Execution**
   - Initializes segment registers (DS, ES, SS, CS all to 0)
   - Sets up stack at 0x7C00
   - Detects disk geometry using BIOS
   - Calculates and reads root directory
   - Searches for "KERNEL  BIN" file
   - Reads File Allocation Table
   - Follows FAT chain to load all kernel clusters
   - Loads kernel to memory at 0x2000:0x0000
   - Jumps to kernel

4. **Kernel Execution**
   - Sets up its own segment registers
   - Prints "Hello world from kernel!"
   - Halts CPU

### Memory Map During Execution

```
0x0000:0x0000 - 0x0000:0x03FF   Interrupt Vector Table (1 KB)
0x0000:0x0400 - 0x0000:0x04FF   BIOS Data Area (256 bytes)
0x0000:0x0500 - 0x0000:0x7BFF   Free conventional memory (~30 KB)
0x0000:0x7C00 - 0x0000:0x7DFF   Bootloader (512 bytes)
0x0000:0x7E00 - 0x0000:0xFFFF   Buffer and stack (~32.5 KB)
0x1000:0x0000 - 0x1FFF:0xFFFF   Free memory (~64 KB)
0x2000:0x0000 - 0x2FFF:0xFFFF   Kernel (up to 64 KB)
0x3000:0x0000 - 0x9FFF:0xFFFF   Free memory (~896 KB)
0xA000:0x0000 - 0xFFFF:0xFFFF   Video memory, BIOS ROM, etc.
```

### File System Structure

The 1.44MB floppy disk image layout:

```
Sector 0:       Boot sector (bootloader code + FAT12 BPB)
Sectors 1-9:    First FAT copy
Sectors 10-18:  Second FAT copy (redundancy)
Sectors 19-32:  Root directory (224 entries max)
Sectors 33+:    Data area (clusters start here)
```

### FAT12 File Allocation

Example: kernel.bin is 3 sectors (1536 bytes)

1. Root directory entry points to first cluster (e.g., cluster 2)
2. FAT entry for cluster 2 points to cluster 3
3. FAT entry for cluster 3 points to cluster 4
4. FAT entry for cluster 4 contains 0xFFF (end of file)

The bootloader follows this chain to load all parts of the kernel.

---

## Building and Running

### Prerequisites

- **NASM** (Netwide Assembler): `sudo apt install nasm`
- **GCC** (GNU Compiler Collection): Usually pre-installed on Linux
- **mtools**: `sudo apt install mtools` (for mcopy)
- **QEMU** (optional, for testing): `sudo apt install qemu-system-x86`

### Building

```bash
# Build everything
make

# Build only the floppy image
make floppy_image

# Build only the FAT utility
make tools_fat

# Clean build artifacts
make clean
```

### Running in QEMU

```bash
# Using the provided script
./run.sh

# Or manually
qemu-system-i386 -fda build/main_floppy.img
```

### Using the FAT Utility

```bash
# Read a file from the disk image
./build/tools/fat build/main_floppy.img "TEST    TXT"

# Read the kernel
./build/tools/fat build/main_floppy.img "KERNEL  BIN"
```

**Note:** Filenames must be in 8.3 format with spaces, e.g., "KERNEL  BIN" not "kernel.bin"

---

## Technical Details

### Why FAT12?

FAT12 is the simplest filesystem:
- Easy to implement
- Well-documented
- Supported by all operating systems
- Perfect for learning OS development

### Real Mode vs Protected Mode

This OS runs entirely in **16-bit real mode**:
- Direct hardware access
- 1 MB memory limit (only 640 KB usable)
- No memory protection
- Simple to program but unsafe

Modern OSes use **32-bit protected mode** or **64-bit long mode** which provide:
- Memory protection
- Virtual memory
- Access to all system memory
- Hardware-assisted multitasking

### Why 512-byte Boot Sector Limit?

Historical reasons:
- Original IBM PC BIOS specification
- One sector was considered sufficient for bootstrap code
- Real bootloaders (like GRUB) use multi-stage loading

### Common Issues and Solutions

**Issue:** `make: nasm: No such file or directory`
- **Solution:** Install NASM: `sudo apt install nasm`

**Issue:** `mcopy: command not found`
- **Solution:** Install mtools: `sudo apt install mtools`

**Issue:** `KERNEL.BIN not found!` when booting
- **Solution:** Ensure kernel.bin was copied correctly. Check with:
  ```bash
  mdir -i build/main_floppy.img ::/
  ```

**Issue:** Bootloader hangs or resets
- **Solution:** Check boot signature (last 2 bytes must be 0xAA55)

### Limitations and Known Issues

1. **64 KB Kernel Limit**: The bootloader can't load kernels larger than 64 KB due to segment limitations (mentioned in code comments)

2. **Hard-coded Cluster Offset**: Line 133 of boot.asm uses hard-coded `+31` instead of calculating dynamically

3. **No Error Recovery**: If disk read fails after retries, system simply reboots

4. **No File System Write Support**: The FAT utility can only read files, not write them

5. **Single-tasking**: Can only run one program (the kernel)

6. **No User Input**: Kernel doesn't handle keyboard input beyond "press any key to reboot" on error

### Future Enhancement Ideas

- Implement a proper 32-bit protected mode kernel
- Add keyboard and mouse drivers
- Implement a simple shell
- Add support for loading and executing programs
- Implement a memory manager
- Add multitasking support
- Create a simple GUI
- Support additional filesystems (FAT16, FAT32)

---

## Learning Resources

This project follows common OS development tutorials and references:

- **OSDev Wiki**: https://wiki.osdev.org/
- **Intel x86 Documentation**: CPU instruction set and architecture manuals
- **BIOS Interrupt Reference**: INT 10h (video), INT 13h (disk), INT 16h (keyboard)
- **FAT Filesystem Specification**: Microsoft's FAT file system documentation

---

## License

See LICENSE file for details.

---

## Acknowledgments

This project was built by following various online tutorials and resources. The code includes comments mentioning "he forgot this in his video," indicating it's based on video tutorials where some details were initially missed and later corrected.

The journey of building an OS from scratch is challenging but extremely educational, providing deep insights into:
- How computers boot
- How filesystems work
- How operating systems manage hardware
- Low-level programming in assembly language
- The interface between software and hardware

Happy OS development! 🚀
