# project_OS - A Custom Operating System from Scratch

My journey of scouring the depths of the internet to learn and build my very own OS (hopefully)

## Table of Contents
1. [Project Overview](#project-overview)
2. [System Architecture](#system-architecture)
3. [Detailed Component Breakdown](#detailed-component-breakdown)
   - [Stage 1 Bootloader (boot.asm)](#stage-1-bootloader-bootasm)
   - [Stage 2 Bootloader (main.asm + main.c)](#stage-2-bootloader-mainasm--mainc)
   - [C Standard Library Components](#c-standard-library-components)
   - [x86 Assembly Helpers](#x86-assembly-helpers)
   - [Kernel (main.asm)](#kernel-mainasm)
   - [FAT12 Utility (fat.c)](#fat12-utility-fatc)
   - [Build System (Makefile)](#build-system-makefile)
4. [How Everything Works Together](#how-everything-works-together)
5. [Building and Running](#building-and-running)
6. [Technical Details](#technical-details)

---

## Project Overview

This is a custom operating system built from scratch that demonstrates fundamental OS development concepts. The project has evolved to include:

- **A two-stage bootloader architecture** - Stage 1 loads Stage 2, enabling more sophisticated boot logic
- **Stage 2 bootloader written in mixed Assembly and C** - Combines low-level hardware control with high-level logic
- **Open Watcom C compiler integration** - Allows writing bootloader code in C with 16-bit real mode support
- **Custom x86 assembly routines** - BIOS interrupt wrappers callable from C
- **A simple kernel** that displays a message when loaded
- **A FAT12 filesystem utility** for reading files from the disk image
- **A modular build system** with separate Makefiles for each component

The OS is designed to run on x86 architecture (16-bit real mode) and can be executed in emulators like QEMU or on real hardware.

### Why Two-Stage Bootloader?

The BIOS boot sector is limited to 512 bytes, which is insufficient for complex bootloader logic. A two-stage design solves this:

1. **Stage 1 (512 bytes)**: Minimal assembly code that understands FAT12 and loads Stage 2
2. **Stage 2 (unlimited size)**: Full-featured bootloader written in C that can perform complex initialization, display messages, load drivers, etc.

This architecture is used by real-world bootloaders like GRUB and Windows Boot Manager.

---

## System Architecture

### Boot Process Flow

```text
Power On → BIOS → Stage 1 Bootloader (512 bytes) → Stage 2 Bootloader (C + ASM) → Kernel → OS Running
```

1. **BIOS Stage**: When the computer starts, the BIOS loads the first sector (512 bytes) of the boot disk into memory at address `0x7C00` and executes it
2. **Stage 1 Bootloader**: Minimal assembly code that initializes hardware, reads the FAT12 filesystem, locates `STAGE2.BIN`, loads it into memory at `0x0500`, and transfers control to it
3. **Stage 2 Bootloader**: More sophisticated bootloader written in mixed C and assembly that can perform complex initialization, display messages, and prepare the system for the kernel
4. **Kernel Stage**: The kernel takes over and runs the operating system

### Memory Layout

```text
0x0000:0x0000 - Interrupt Vector Table
0x0000:0x0500 - Stage 2 bootloader loaded here
0x0000:0x7C00 - Stage 1 bootloader loaded here (512 bytes)
0x0000:0x7E00 - Buffer for disk operations
0x2000:0x0000 - Kernel loaded here (future)
```

### Key Advantages of Two-Stage Design

- **More Space**: Stage 2 can be any size, not limited to 512 bytes
- **High-Level Language**: Stage 2 can be written in C for better maintainability
- **Error Handling**: Room for detailed error messages and diagnostics
- **Modularity**: Can load multiple components (drivers, config files, etc.)
- **Flexibility**: Easy to add features without fighting size constraints

---

## Detailed Component Breakdown

### Stage 1 Bootloader (boot.asm)

Stage 1 is the 512-byte bootloader that the BIOS loads first. Its sole job is to load Stage 2 from the FAT12 filesystem.

**Key responsibilities:**
- Initialize segment registers and stack
- Detect disk geometry using BIOS
- Read and parse FAT12 filesystem structures
- Search for `STAGE2.BIN` in the root directory
- Load Stage 2 into memory at `0x0500`
- Transfer control to Stage 2

The implementation is nearly identical to the full bootloader from the previous version, except:
- Searches for `"STAGE2  BIN"` instead of `"KERNEL  BIN"`
- Loads to memory address `0x0500` instead of `0x2000`
- Uses constants `STAGE2_LOAD_SEGMENT = 0x0` and `STAGE2_LOAD_OFFSET = 0x500`

**Why load at 0x0500?**
- Address `0x0500-0x7BFF` is guaranteed free memory (BIOS Data Area ends at 0x500)
- Avoids conflicts with the Stage 1 bootloader at 0x7C00
- Provides ~30 KB of space for Stage 2 code
- Standard location used by many bootloaders

See the [original bootloader documentation](#bootloader-deep-dive-legacy) below for detailed explanation of FAT12 reading, LBA to CHS conversion, and disk I/O routines.

---

### Stage 2 Bootloader (main.asm + main.c)

Stage 2 is a sophisticated bootloader written in mixed assembly and C, compiled with Open Watcom's 16-bit compiler.

#### Architecture Overview

**Linker Memory Map** (`stage2.map`):
```text
Segment                Address    Size
=======                =======    ====
_ENTRY (assembly)      0x0000     0x0013  (19 bytes)
_TEXT (C code)         0x0013     0x000F  (15 bytes)
CONST (constants)      0x0022     0x0000
_DATA (variables)      0x0022     0x0000

Entry point: 0x0000 (first byte of stage2.bin)
```

The linker creates a raw binary where execution starts at byte 0, allowing Stage 1 to jump directly to the beginning of Stage 2.

#### main.asm - Assembly Entry Point

```assembly
bits 16
section _ENTRY class=CODE

extern _cstart_
global entry

entry:
    cli                    ; Disable interrupts during setup
    
    ; Setup segments
    mov ax, ds
    mov ss, ax
    mov sp, 0             ; SP = 0 (stack starts at segment:0)
    mov bp, sp            ; BP = 0 (base pointer)
    
    sti                   ; Re-enable interrupts
    
    ; Pass boot drive to C function
    xor dh, dh            ; Clear upper byte (DL contains boot drive from Stage 1)
    push dx               ; Push boot drive as parameter
    call _cstart_         ; Call C entry point
    
    cli                   ; If C returns (shouldn't happen), halt
    hlt
```

**Key features:**
- **Segment initialization**: Sets up segment registers for C code execution
- **Stack setup**: Establishes a stack for C function calls
- **C function call**: Jumps to `_cstart_()` in main.c
- **Halt on return**: If C code returns, halt the system

**Open Watcom calling convention:**
- C functions are prefixed with underscore (`cstart_` becomes `_cstart_`)
- Parameters passed on stack (right-to-left)
- Caller cleans up stack
- Return value in AX register

#### main.c - C Entry Point

```c
#include "stdint.h"
#include "stdio.h"

void _cdecl cstart_(uint16_t bootDrive)
{
    puts("Hello world from C!");
    for(;;);
}
```

**Important Note**: The function includes an infinite loop `for(;;)` to prevent returning to the assembly code, as the kernel loading functionality is not yet implemented.

**Compiler**: Open Watcom C Compiler (wcc)
- **Target**: 16-bit real mode (80186/80286 compatible)
- **Memory model**: Small model (`-ms`)
  - Code and data fit in 64 KB
  - Near pointers only (16-bit offsets)
  - Fast and efficient for small programs
- **Calling convention**: `_cdecl` - C declaration, parameters on stack
- **No standard library**: `-zl` flag disables linking with standard library
- **Optimizations**: `-s` removes stack overflow checks, `-zq` quiet mode

**Compilation command:**
```bash
wcc -4 -d3 -s -wx -ms -zl -zq -fo=output.obj main.c
```

Flags explained:
- `-4`: Generate 80386 instructions (backwards compatible)
- `-d3`: Full debugging information
- `-s`: Remove stack overflow checking (saves space)
- `-wx`: Maximum warning level
- `-ms`: Small memory model
- `-zl`: No default library searching
- `-zq`: Quiet operation

#### Linking Stage 2

**Linker**: Open Watcom Linker (wlink)

```bash
wlink NAME stage2.bin \
      FILE { main_asm.obj main_c.obj x86.obj stdio.obj } \
      OPTION MAP=stage2.map \
      @linker.lnk
```

**linker.lnk configuration:**
```text
FORMAT RAW BIN
OPTION  QUIET,
        NODEFAULTLIBS,
        START=entry,
        VERBOSE,
        OFFSET=0,
        STACK=0x200
ORDER
    CLNAME CODE
        SEGMENT _ENTRY
        SEGMENT _TEXT
    CLNAME DATA
```

**Configuration details:**

- **`FORMAT RAW BIN`**: Output raw binary, not ELF/COFF executable
- **`QUIET`**: Suppress unnecessary linker messages
- **`NODEFAULTLIBS`**: Don't link with C runtime library
- **`START=entry`**: Entry point is the `entry` symbol in main.asm
- **`VERBOSE`**: Enable verbose output for debugging link process
- **`OFFSET=0`**: Start addresses at 0 (Stage 1 sets segment registers)
- **`STACK=0x200`**: Reserves 512 bytes for stack (though we manually set SS:SP)
- **`ORDER`**: Specifies segment order in the output binary
  - `_ENTRY` first (assembly entry point)
  - `_TEXT` second (C code)
  - Data segments after code

**Why this order matters:**
- Stage 1 jumps to byte 0 of Stage 2
- Byte 0 must be executable code (the `entry:` label)
- C code follows assembly entry point
- Data sections come after all code

**Warning: "stack segment not found"**
- Expected for raw binaries
- We manually set SS:SP in assembly
- Safe to ignore

---

### C Standard Library Components

Since we're in a bare-metal environment, we must implement our own I/O functions.

#### stdint.h - Standard Integer Types

```c
#pragma once

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
```

**Purpose**: Provides fixed-width integer types for precise memory layout and hardware interaction.

**Open Watcom specifics:**
- `char` = 8 bits
- `short` = 16 bits  
- `long` = 32 bits
- `long long` = 64 bits
- `int` = 16 bits (in 16-bit mode)

#### stdio.h - Standard Input/Output

```c
#pragma once

void putc(char c);
void puts(const char* str);
```

Simple interface for character and string output using BIOS interrupts.

#### stdio.c - Implementation

```c
#include "stdio.h"
#include "x86.h"

void putc(char c)
{
    x86_Video_WriteCharTeletype(c, 0);
}

void puts(const char* str)
{
    while (*str)
    {
        putc(*str);
        str++;
    }
}
```

**How it works:**
- `putc()`: Outputs single character by calling assembly wrapper
- `puts()`: Iterates through null-terminated string, calling `putc()` for each character
- No buffering (writes directly to screen via BIOS)
- No newline handling (must include `\n` explicitly)

---

### x86 Assembly Helpers

To call BIOS interrupts from C, we need assembly wrappers that follow C calling conventions.

#### x86.h - Function Declarations

```c
#pragma once

#include "stdint.h"

void _cdecl x86_Video_WriteCharTeletype(char c, uint8_t page);
```

**`_cdecl` attribute**: Tells compiler to use C calling convention (caller cleans stack).

#### x86.asm - Assembly Implementation

```assembly
bits 16

section _TEXT class=CODE

global _x86_Video_WriteCharTeletype
_x86_Video_WriteCharTeletype:
    push bp
    mov bp, sp        ; Setup stack frame
    
    push bx           ; Save BX (BIOS may modify it)
    
    ; Get parameters from stack
    mov ah, 0Eh       ; BIOS function: teletype output
    mov al, [bp + 4]  ; Parameter 1: character (at BP+4)
    mov bh, [bp + 6]  ; Parameter 2: page number (at BP+6)
    
    int 10h           ; BIOS video interrupt
    
    pop bx            ; Restore BX
    
    mov sp, bp
    pop bp
    ret               ; Caller cleans up parameters
```

**Open Watcom stack frame (16-bit, small model):**

```text
[BP + 6]  - Parameter 2 (page)
[BP + 4]  - Parameter 1 (character)
[BP + 2]  - Return address (offset)
[BP + 0]  - Saved BP
[BP - 2]  - Local variables (if any)
```

**Why this layout?**
- Parameters pushed right-to-left
- Return address pushed by CALL instruction
- BP pushed to preserve caller's stack frame
- Parameters accessed via positive offsets from BP

**BIOS INT 10h, AH=0Eh - Teletype Output:**
- **Input:**
  - AH = 0x0E (function number)
  - AL = Character to display
  - BH = Page number (usually 0)
  - BL = Foreground color (in graphics modes)
- **Effect:**
  - Displays character at cursor position
  - Advances cursor
  - Handles special characters (\n, \r, \b, \a)
  - Scrolls screen if needed

**Why save/restore BX?**
- BIOS may modify registers
- C calling convention requires preserving BX, SI, DI, BP
- AX, CX, DX are scratch registers (caller saves if needed)

---

### Kernel (main.asm)

The kernel is currently a simple demonstration that prints a message. In the future, this will be replaced with a full-featured operating system kernel.

**Current implementation:**

```assembly
org 0x0
bits 16

start:
    mov ax, 0x2000     ; Segment where kernel would be loaded
    mov ds, ax
    mov es, ax
    
    mov si, msg_hello
    call puts
    
.halt:
    cli                ; Disable interrupts
    hlt                ; Halt CPU
```

The kernel would be loaded by Stage 2 (not yet implemented) at memory address `0x2000:0x0000`.

---

### Build System Components

The project uses a modular build system with separate Makefiles for each component.

#### Main Makefile Structure

**Variables:**
```makefile
ASM=nasm                 # Netwide Assembler
CC=gcc                   # GNU C Compiler (for tools)
CC16=wcc                 # Open Watcom C Compiler (for 16-bit code)
LD16=wlink               # Open Watcom Linker
SRC_DIR=src
TOOLS_DIR=tools
BUILD_DIR=build
```

**Build targets:**

1. **Stage 1 Bootloader**
   ```makefile
   $(BUILD_DIR)/stage1.bin: src/bootloader/stage1/boot.asm
       nasm boot.asm -f bin -o $(BUILD_DIR)/stage1.bin
   ```
   Assembles Stage 1 to raw binary.

2. **Stage 2 Bootloader**
   ```makefile
   $(BUILD_DIR)/stage2.bin: main.asm main.c stdio.c x86.asm
       nasm -f obj -o main.obj main.asm
       nasm -f obj -o x86.obj x86.asm
       wcc -4 -d3 -s -wx -ms -zl -zq -fo=main_c.obj main.c
       wcc -4 -d3 -s -wx -ms -zl -zq -fo=stdio.obj stdio.c
       wlink NAME stage2.bin FILE { main.obj x86.obj main_c.obj stdio.obj } @linker.lnk
   ```
   
   **Process:**
   - Assemble .asm files to OBJ format (not raw binary)
   - Compile .c files to OBJ format
   - Link all OBJ files together with custom linker script
   - Output raw binary with entry point at byte 0

3. **Floppy Image Creation**
   ```makefile
   $(BUILD_DIR)/main_floppy.img: stage1 stage2
       dd if=/dev/zero of=$@ bs=512 count=2880
       mkfs.fat -F 12 -n "NBOS" $@
       dd if=$(BUILD_DIR)/stage1.bin of=$@ conv=notrunc
       mcopy -i $@ $(BUILD_DIR)/stage2.bin "::/stage2.bin"
       mcopy -i $@ test.txt "::/test.txt"
   ```
   
   **Steps:**
   - Create blank 1.44 MB image
   - Format as FAT12 filesystem
   - Overwrite boot sector with Stage 1
   - Copy Stage 2 and test files into filesystem

#### Sub-Makefiles

**stage1/Makefile:**
```makefile
BUILD_DIR ?= $(abspath build)

.PHONY: all clean

all: $(BUILD_DIR)/stage1.bin

$(BUILD_DIR)/stage1.bin: boot.asm
	nasm boot.asm -f bin -o $@

clean:
	rm -f $(BUILD_DIR)/stage1.bin
```

**stage2/Makefile:**
```makefile
BUILD_DIR ?= $(abspath build)

ASM=nasm
CC16=wcc
LD16=wlink

SOURCES_ASM=$(wildcard *.asm)
SOURCES_C=$(wildcard *.c)

OBJECTS_ASM=$(patsubst %.asm, $(BUILD_DIR)/stage2/asm/%.obj, $(SOURCES_ASM))
OBJECTS_C=$(patsubst %.c, $(BUILD_DIR)/stage2/c/%.obj, $(SOURCES_C))

.PHONY: all clean

all: $(BUILD_DIR)/stage2.bin

$(BUILD_DIR)/stage2.bin: $(OBJECTS_ASM) $(OBJECTS_C)
	$(LD16) NAME $@ FILE { $(OBJECTS_ASM) $(OBJECTS_C) } OPTION MAP=$(BUILD_DIR)/stage2.map @linker.lnk

$(BUILD_DIR)/stage2/asm/%.obj: %.asm
	mkdir -p $(BUILD_DIR)/stage2/asm
	$(ASM) -f obj -o $@ $<

$(BUILD_DIR)/stage2/c/%.obj: %.c
	mkdir -p $(BUILD_DIR)/stage2/c
	$(CC16) -4 -d3 -s -wx -ms -zl -zq -fo=$@ $<

clean:
	rm -f $(BUILD_DIR)/stage2.bin
```

**Key features:**
- **Automatic dependency tracking**: Wildcard patterns find all .asm and .c files
- **Separate object directories**: Assembly and C objects stored separately
- **Pattern rules**: Generic rules for compiling any .c or .asm file
- **BUILD_DIR parameter**: Parent Makefile can override build location

---

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

### Complete Boot Sequence (Two-Stage Architecture)

1. **Power On**
   - BIOS performs POST (Power-On Self Test)
   - BIOS identifies bootable drives
   - BIOS loads first sector (512 bytes) from boot disk to `0x7C00`
   - BIOS verifies boot signature (0xAA55)
   - BIOS jumps to `0x7C00`

2. **Stage 1 Bootloader Execution**
   - **Initialize hardware** (Lines 38-51)
     - Set DS, ES, SS, CS all to 0 for linear addressing
     - Set stack pointer to `0x7C00` (grows downward)
   - **Detect disk geometry** (Lines 54-72)
     - BIOS INT 13h, AH=08h to get sectors/track and heads
     - Saves geometry for LBA→CHS conversion
   - **Read FAT12 structures** (Lines 74-95)
     - Calculate root directory location (sector 19)
     - Calculate root directory size (14 sectors)
     - Load root directory to buffer at `0x7E00`
   - **Search for Stage 2** (Lines 98-114)
     - Linear search through 224 directory entries
     - Look for filename `"STAGE2  BIN"` (11 characters, space-padded)
     - If not found, display error and reboot
   - **Load File Allocation Table** (Lines 116-125)
     - FAT starts at sector 1, is 9 sectors long
     - Load entire FAT to memory buffer
   - **Load Stage 2 into memory** (Lines 127-174)
     - Get first cluster from directory entry (offset +26)
     - Loop: Read cluster, follow FAT chain, repeat until EOF (0xFF8)
     - Load to memory at `0x0000:0x0500`
   - **Transfer control to Stage 2** (Lines 168-174)
     - Pass boot drive number in DL
     - Far jump to `0x0000:0x0500`

3. **Stage 2 Bootloader Execution**
   - **Assembly entry point** (`main.asm`)
     - Disable interrupts during initialization
     - Setup segments (DS, SS)
     - Setup stack (SP = 0)
     - Enable interrupts
     - Call C entry point `_cstart_()`
   - **C code execution** (`main.c`)
     - Receives boot drive number as parameter
     - Calls `puts("Hello world from C!_")`
     - `puts()` calls `putc()` for each character
     - `putc()` calls assembly wrapper `x86_Video_WriteCharTeletype()`
     - Assembly wrapper invokes BIOS INT 10h to display characters
   - **Screen output visible in QEMU**
     - Message displayed via BIOS teletype function
     - Cursor advances automatically
   - **Return and halt**
     - If C code returns, assembly entry point halts CPU

4. **Future: Kernel Execution** (not yet implemented)
   - Stage 2 would load `KERNEL.BIN` from FAT12
   - Transfer control to kernel at `0x2000:0x0000`
   - Kernel initializes hardware, memory management, etc.

### Memory Map During Execution

```text
Address Range                      Contents                                  Size      Access
================================================================================================
0x0000:0x0000 - 0x0000:0x03FF     Interrupt Vector Table (IVT)              1 KB      Read/Write
0x0000:0x0400 - 0x0000:0x04FF     BIOS Data Area (BDA)                      256 bytes Read/Write
0x0000:0x0500 - 0x0000:0x7BFF     Stage 2 Bootloader (loaded here)          ~29.5 KB  Read/Execute
0x0000:0x7C00 - 0x0000:0x7DFF     Stage 1 Bootloader (BIOS loads here)      512 bytes Read/Execute
0x0000:0x7E00 - 0x0000:0xFFFF     FAT buffer, stack, free memory            ~32.5 KB  Read/Write
0x1000:0x0000 - 0x1FFF:0xFFFF     Free conventional memory                  ~64 KB    Available
0x2000:0x0000 - 0x2FFF:0xFFFF     Kernel (future, not yet used)             up to 64KB Available
0x3000:0x0000 - 0x9FFF:0xFFFF     Free conventional memory                  ~896 KB   Available
0xA000:0x0000 - 0xBFFF:0xFFFF     Video memory (text mode @ B8000)          128 KB    Read/Write
0xC000:0x0000 - 0xFFFF:0xFFFF     BIOS ROM, hardware mappings               256 KB    Read-only
```

**Actual Memory Usage During Boot:**

1. **Stage 1 Execution (at 0x7C00):**
   - Code: 512 bytes at 0x7C00
   - Stack: Grows down from 0x7C00 (uses ~100 bytes typically)
   - Disk buffer: At 0x7E00 (used for reading FAT and root directory)
   - Stage 1 reads FAT12 structures into buffer starting at 0x7E00

2. **Stage 2 Loaded (at 0x0500):**
   - Entry point: 0x0000:0x0500 (first byte of stage2.bin)
   - Current size: ~34 bytes (main.asm + main.c + stdio.c + x86.asm compiled)
   - Can grow up to ~29 KB before hitting Stage 1
   - Stack: Set to segment:0 (SP=0), grows downward wrapping to high memory

3. **Stack Configuration:**
   - Stage 1: SS=0x0000, SP=0x7C00 (stack from 0x7C00 down to ~0x7B00)
   - Stage 2: SS=DS (wherever bootloader was loaded), SP=0x0000 (wraps around)
   - Stack usage: Minimal (~50 bytes for function calls and parameters)

**Key memory regions:**
- **IVT (0x0000-0x03FF)**: 256 interrupt vector entries (4 bytes each)
- **BDA (0x0400-0x04FF)**: BIOS data (detected hardware, keyboard buffer, etc.)
- **Free (0x0500-0x7BFF)**: Safe to use; Stage 2 loaded here
- **Stage 1 (0x7C00-0x7DFF)**: Still in memory but can be overwritten after Stage 2 loads
- **Buffer (0x7E00+)**: Used by Stage 1 for disk reads; available after loading
- **Video (0xB8000)**: Text mode video memory (80x25 characters, 2 bytes each)

### Stack Usage

**Stage 1 stack:**
- Starts at `0x7C00`, grows downward toward `0x7E00` buffer
- Small stack usage (< 100 bytes typically)
- Used for function calls and local variables

**Stage 2 stack:**
- Starts at `0x0000` (SP=0), grows downward
- Wraps around to high memory (0xFFFF)
- C code requires more stack space for local variables
- ~64 KB available before hitting Stage 1 at 0x7C00

### Disk Image Structure

**1.44 MB Floppy Disk Layout:**

```text
Sector Range    Component                   Size        Purpose
==================================================================================
0               Boot Sector                 512 bytes   Stage 1 bootloader + BPB
1-9             FAT #1                      4608 bytes  File Allocation Table (primary)
10-18           FAT #2                      4608 bytes  File Allocation Table (backup)
19-32           Root Directory              7168 bytes  Up to 224 file entries
33-2879         Data Area                   1.4 MB      File contents (clusters)
```

### Disk Image Structure

**1.44 MB Floppy Disk Layout:**

```text
Sector Range    Component                   Size        Purpose
==================================================================================
0               Boot Sector                 512 bytes   Stage 1 bootloader + BPB
1-9             FAT #1                      4608 bytes  File Allocation Table (primary)
10-18           FAT #2                      4608 bytes  File Allocation Table (backup)
19-32           Root Directory              7168 bytes  Up to 224 file entries
33-2879         Data Area                   1.4 MB      File contents (clusters)
```

**Files in the disk image:**
When you build the OS, these files are automatically copied to the disk image:
- `STAGE2.BIN` - Stage 2 bootloader (loaded by Stage 1)
- `TEST.TXT` - Sample text file (for testing FAT12 reading)

You can verify the contents using the FAT utility:
```bash
./build/tools/fat build/main_floppy.img "STAGE2  BIN"
./build/tools/fat build/main_floppy.img "TEST    TXT"
```

Or using mtools:
```bash
mdir -i build/main_floppy.img ::/
mtype -i build/main_floppy.img ::/test.txt
```

**FAT12 Calculations:**
- **Total sectors**: 2880 (1,474,560 bytes = 1.44 MB)
- **Root directory entries**: 224 entries × 32 bytes = 7168 bytes = 14 sectors
- **FAT size**: 2880 sectors × 1.5 bytes/entry = 4320 bytes = 9 sectors (rounded up)
- **Data area starts**: 1 + 9 + 9 + 14 = sector 33
- **Available clusters**: (2880 - 33) / 1 = 2847 clusters

**Why two FATs?**
- Redundancy: If one gets corrupted, the other can be used
- Standard FAT12 specification requires it
- Tools like `fsck` can repair using backup FAT

### File Allocation Table (FAT12)

**How FAT12 works:**

FAT12 uses 12 bits (1.5 bytes) per cluster entry. Entries are packed:

```text
Byte offset:  0     1     2     3     4     5     6
              ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┐
              │  C0 │C1|C0│  C1 │  C2 │C3|C2│  C3 │ ... │
              └─────┴─────┴─────┴─────┴─────┴─────┴─────┘
Bit layout:   <-12->  <-12->  <-12->  <-12->  <-12->
              Cluster Cluster Cluster Cluster Cluster
                 0       1       2       3       4
```

**Example: Reading cluster 5**
- Byte offset = 5 × 3 / 2 = 7.5 → byte 7
- Load 16 bits from byte 7
- Since cluster is odd: shift right 4 bits
- Result = next cluster number (or 0xFF8-0xFFF for EOF)

**Special FAT values:**
- `0x000`: Free cluster
- `0x001`: Reserved
- `0x002-0xFF6`: Points to next cluster in chain
- `0xFF7`: Bad cluster (hardware defect)
- `0xFF8-0xFFF`: End of file marker

**Example file chain:**
```text
File: STAGE2.BIN (3 clusters)

Root Directory Entry:
  FirstCluster = 2

FAT entries:
  FAT[2] = 3    → Continue to cluster 3
  FAT[3] = 4    → Continue to cluster 4
  FAT[4] = 0xFFF → End of file
```

### C Code Integration Details

**Why Open Watcom?**
- One of few compilers still supporting 16-bit x86 targets
- Can generate real mode code
- Produces small, efficient binaries
- Active maintenance and good documentation
- Alternatives (Turbo C, Borland C) are obsolete

**Memory models:**
- **Tiny**: Code+data in 64 KB, CS=DS=SS
- **Small**: Code in 64 KB, data in 64 KB (separate segments)
- **Compact**: Code in 64 KB, data can span multiple 64 KB segments
- **Medium**: Code can span segments, data in 64 KB
- **Large**: Both code and data can span segments
- **Huge**: Like large but single arrays can exceed 64 KB

**We use Small model** because:
- Stage 2 code < 64 KB
- Stage 2 data < 64 KB
- Simplest, fastest model
- Near pointers only (no segment manipulation needed)

**Calling convention (_cdecl):**
```text
Stack before call:
┌──────────────┐
│  Parameter N │  ← BP + 2N+2
├──────────────┤
│     ...      │
├──────────────┤
│  Parameter 2 │  ← BP + 6
├──────────────┤
│  Parameter 1 │  ← BP + 4
├──────────────┤
│ Return Addr  │  ← BP + 2
├──────────────┤
│  Saved BP    │  ← BP (current base pointer)
├──────────────┤
│   Locals     │  ← BP - 2, BP - 4, ...
└──────────────┘  ← SP (stack pointer)
```

**Why this matters:**
- C compiler generates code assuming this layout
- Assembly wrappers must match this convention
- Misalignment causes corrupted parameters/crashes

---

## Building and Running

### What You'll See When Running the OS

When you run this OS in QEMU (or on real hardware), here's what actually happens:

**Boot Sequence Timeline:**

1. **QEMU Window Opens** - Black screen appears
2. **Stage 1 Executes** (~0.1 seconds)
   - BIOS loads first 512 bytes to memory
   - Stage 1 bootloader initializes disk
   - Reads FAT12 filesystem structure
   - Locates and loads `STAGE2.BIN`
   - No visible output (happens too fast to see)
3. **Stage 2 Executes** (~0.1 seconds)
   - Assembly entry point sets up environment
   - Calls C code
   - **Screen displays: "Hello world from C!"**
   - System halts (infinite loop in C code)

**What the screen looks like:**
```
Hello world from C!█
```
(Where █ represents the cursor)

The OS successfully demonstrates:
- ✅ Two-stage bootloader architecture working
- ✅ FAT12 filesystem reading operational
- ✅ C code executing in real mode
- ✅ BIOS interrupt calls from C (via assembly wrappers)
- ✅ Screen output working

**What's NOT yet implemented:**
- ❌ Kernel loading (Stage 2 doesn't load kernel yet)
- ❌ Keyboard input handling
- ❌ Protected mode transition
- ❌ Memory management
- ❌ Any actual OS functionality

### Prerequisites

**Required tools:**
- **NASM** (Netwide Assembler): `sudo apt install nasm` or `sudo dnf install nasm`
- **Open Watcom C/C++**: Download from https://open-watcom.github.io/
  - Install to `/usr/local/watcom` or set `WATCOM` environment variable
  - Add `$WATCOM/binl64` (or `binl`) to PATH
- **mtools**: `sudo apt install mtools` or `sudo dnf install mtools`
- **GCC**: Usually pre-installed (for building FAT utility)

**Optional tools:**
- **QEMU**: `sudo apt install qemu-system-x86` or `sudo dnf install qemu-system-x86`
- **GNU Make**: Usually pre-installed

**Verifying installation:**
```bash
nasm -version          # Should show NASM version
wcc -?                 # Should show Watcom C compiler help
wlink -?               # Should show Watcom linker help
mcopy -V               # Should show mtools version
qemu-system-i386 --version  # Should show QEMU version
```

### Building

**Complete Build Process Explained:**

When you run `make`, here's exactly what happens:

```bash
# Build everything (Stage 1, Stage 2, kernel, floppy image, tools)
make
```

**Step-by-step build process:**

1. **Create build directory**
   ```bash
   mkdir -p build
   ```

2. **Build Stage 1 Bootloader**
   ```bash
   nasm src/bootloader/stage1/boot.asm -f bin -o build/stage1.bin
   ```
   - Output: `build/stage1.bin` (exactly 512 bytes)
   - Contains: FAT12 header + bootloader code + boot signature (0xAA55)

3. **Build Stage 2 Bootloader**
   
   a. Assemble assembly files to OBJ format:
   ```bash
   nasm -f obj -o build/stage2/asm/main.obj src/bootloader/stage2/main.asm
   nasm -f obj -o build/stage2/asm/x86.obj src/bootloader/stage2/x86.asm
   ```
   
   b. Compile C files to OBJ format:
   ```bash
   wcc -4 -d3 -s -wx -ms -zl -zq -fo=build/stage2/c/main.obj src/bootloader/stage2/main.c
   wcc -4 -d3 -s -wx -ms -zl -zq -fo=build/stage2/c/stdio.obj src/bootloader/stage2/stdio.c
   ```
   
   c. Link all OBJ files:
   ```bash
   wlink NAME build/stage2.bin \
         FILE { build/stage2/asm/main.obj build/stage2/asm/x86.obj \
                build/stage2/c/main.obj build/stage2/c/stdio.obj } \
         OPTION MAP=build/stage2.map \
         @src/bootloader/stage2/linker.lnk
   ```
   - Output: `build/stage2.bin` (raw binary, currently ~34 bytes)
   - Also creates: `build/stage2.map` (linker map file for debugging)

4. **Build FAT12 Utility**
   ```bash
   gcc -g -o build/tools/fat tools/fat/fat.c
   ```
   - Output: `build/tools/fat` (executable for reading FAT12 disk images)

5. **Create Bootable Floppy Image**
   
   a. Create blank 1.44 MB image:
   ```bash
   dd if=/dev/zero of=build/main_floppy.img bs=512 count=2880
   ```
   - Creates: 1,474,560 bytes (1.44 MB) of zeros
   
   b. Format as FAT12:
   ```bash
   mkfs.fat -F 12 -n "NBOS" build/main_floppy.img
   ```
   - Creates FAT12 filesystem structure
   - Volume label: "NBOS"
   
   c. Install Stage 1 bootloader:
   ```bash
   dd if=build/stage1.bin of=build/main_floppy.img conv=notrunc
   ```
   - Overwrites first 512 bytes with bootloader
   - `conv=notrunc`: Preserves rest of file (FAT12 structures)
   
   d. Copy Stage 2 to filesystem:
   ```bash
   mcopy -i build/main_floppy.img build/stage2.bin "::/stage2.bin"
   ```
   - Copies Stage 2 into FAT12 filesystem
   - Filename becomes "STAGE2  BIN" (8.3 format)
   
   e. Copy test file:
   ```bash
   mcopy -i build/main_floppy.img test.txt "::/test.txt"
   ```
   - Copies test.txt for FAT12 read testing

**Individual build targets:**

```bash
# Build only Stage 1 bootloader
make stage1

# Build only Stage 2 bootloader
make stage2

# Build floppy image (requires stage1, stage2, kernel)
make floppy_image

# Build FAT utility
make tools_fat

# Clean all build artifacts
make clean
```

**Build output:**
```text
build/
├── stage1.bin          # Stage 1 bootloader (512 bytes exactly)
├── stage2.bin          # Stage 2 bootloader (raw binary, ~34 bytes currently)
├── stage2.map          # Linker map file (for debugging)
├── main_floppy.img     # Bootable 1.44 MB floppy image
├── stage2/
│   ├── asm/
│   │   ├── main.obj    # Assembled from main.asm (entry point)
│   │   └── x86.obj     # Assembled from x86.asm (BIOS wrappers)
│   └── c/
│       ├── main.obj    # Compiled from main.c (cstart_ function)
│       └── stdio.obj   # Compiled from stdio.c (putc, puts)
└── tools/
    └── fat             # FAT12 utility executable
```

**Verifying the Build:**

After building, you can verify everything worked correctly:

1. **Check file sizes:**
   ```bash
   ls -lh build/stage1.bin build/stage2.bin build/main_floppy.img
   # stage1.bin should be exactly 512 bytes
   # stage2.bin should be ~34 bytes (will grow as features are added)
   # main_floppy.img should be exactly 1.44 MB (1,474,560 bytes)
   ```

2. **Check boot signature:**
   ```bash
   hexdump -C build/stage1.bin | tail -n 1
   # Should end with: 55 aa (boot signature)
   ```

3. **Verify Stage 1 bootloader:**
   ```bash
   hexdump -C build/stage1.bin | head -n 3
   # First 3 bytes should be: eb XX 90 (jump instruction + nop)
   # Followed by "MSWIN4.1" (OEM identifier)
   ```

4. **List files in disk image:**
   ```bash
   mdir -i build/main_floppy.img ::/
   # Should show:
   #  Volume in drive : is NBOS
   #  Volume Serial Number is XXXX-XXXX
   #  STAGE2   BIN        34 date time
   #  TEST     TXT        26 date time
   ```

5. **Read a file from disk image:**
   ```bash
   ./build/tools/fat build/main_floppy.img "STAGE2  BIN" | hexdump -C
   # Should show the Stage 2 binary content
   
   mtype -i build/main_floppy.img ::/test.txt
   # Should display: This is a test text file
   ```

6. **Inspect linker map:**
   ```bash
   cat build/stage2.map
   # Shows memory layout, segment addresses, and symbol addresses
   ```

**Example linker map output:**
```text
Open Watcom Linker Version X.X
...
                        +------------+
                        |   Groups   |
                        +------------+

Group                           Address              Size
=====                           =======              ====

DGROUP                          00000022             00000000



                        +--------------+
                        |   Segments   |
                        +--------------+

Segment                Class          Group          Address         Size
=======                =====          =====          =======         ====

_ENTRY                 CODE           AUTO           00000000        00000013  <-- Entry point (19 bytes)
_TEXT                  CODE           AUTO           00000013        XXXXXXXX  <-- C code
...
```

This shows _ENTRY starts at offset 0 (correct!) and _TEXT follows immediately after.

### Running in QEMU

**Using the run script:**
```bash
./run.sh
```

**Or manually:**
```bash
qemu-system-i386 -fda build/main_floppy.img
```

**Advanced QEMU options:**
```bash
# Suppress format warning
qemu-system-i386 -drive file=build/main_floppy.img,if=floppy,format=raw

# Enable debugging (GDB stub on port 1234)
qemu-system-i386 -fda build/main_floppy.img -s -S

# Boot with serial console
qemu-system-i386 -fda build/main_floppy.img -serial stdio

# Increase memory
qemu-system-i386 -fda build/main_floppy.img -m 64M
```

### Running on Real Hardware

**Creating a bootable USB:**
```bash
# WARNING: This will erase the USB drive!
# Replace /dev/sdX with your USB device
sudo dd if=build/main_floppy.img of=/dev/sdX bs=512

# Verify
sudo fdisk -l /dev/sdX
```

**Creating a bootable CD:**
```bash
# Create El Torito bootable ISO
genisoimage -b build/main_floppy.img -o bootable.iso -no-emul-boot
```

### Running in VirtualBox

1. Create new virtual machine
2. Type: Other, Version: Other/Unknown
3. Memory: 64 MB (minimum)
4. Disable hard disk
5. Settings → Storage → Add Floppy Controller
6. Add `main_floppy.img` as floppy disk
7. Start VM

### Running in VMware

1. Create new virtual machine
2. Guest OS: Other → Other
3. Remove hard disk
4. Add floppy drive
5. Point to `main_floppy.img`
6. Power on
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

---

## Project Structure and File Descriptions

### Directory Layout

```
project_OS/
├── src/                                 # Source code
│   ├── bootloader/
│   │   ├── boot.asm                    # Legacy bootloader (not used)
│   │   ├── stage1/
│   │   │   └── boot.asm                # Stage 1 bootloader (512 bytes)
│   │   └── stage2/
│   │       ├── main.asm                # Stage 2 entry point (assembly)
│   │       ├── main.c                  # Stage 2 main function (C)
│   │       ├── stdio.h                 # stdio interface
│   │       ├── stdio.c                 # putc and puts implementation
│   │       ├── stdint.h                # Fixed-width integer types
│   │       ├── x86.h                   # x86 BIOS wrapper interface
│   │       ├── x86.asm                 # BIOS INT 10h wrapper
│   │       ├── linker.lnk              # Linker script for Stage 2
│   │       └── Makefile                # Stage 2 build rules
│   └── kernel/
│       ├── main.asm                    # Kernel (not yet used)
│       └── Makefile                    # Kernel build rules
├── tools/
│   └── fat/
│       └── fat.c                       # FAT12 disk image utility
├── build/                              # Build output (created by make)
├── Makefile                            # Main build file
├── run.sh                              # Script to build and run in QEMU
├── test.sh                             # Test script
├── test.txt                            # Test file (copied to disk image)
├── README.md                           # This documentation
└── LICENSE                             # License file
```

### File-by-File Description

#### Bootloader Files

**`src/bootloader/stage1/boot.asm`** (Primary Bootloader, ~500 lines)
- **Purpose:** First 512 bytes loaded by BIOS
- **What it does:**
  1. Initializes segment registers and stack
  2. Detects disk geometry via BIOS INT 13h
  3. Reads FAT12 filesystem structures
  4. Searches for "STAGE2  BIN" in root directory
  5. Follows FAT12 cluster chain to load entire file
  6. Loads Stage 2 to memory at 0x0000:0x0500
  7. Transfers control to Stage 2
- **Size limit:** Exactly 512 bytes (enforced by padding and boot signature)
- **Key features:** FAT12 parser, LBA to CHS conversion, disk retry logic

**`src/bootloader/boot.asm`** (Legacy, ~300 lines)
- **Purpose:** Old single-stage bootloader (before two-stage architecture)
- **Status:** Not used in current build, kept for reference
- **Difference:** Loaded kernel directly instead of Stage 2

**`src/bootloader/stage2/main.asm`** (21 lines)
- **Purpose:** Assembly entry point for Stage 2
- **What it does:**
  1. Disables interrupts during setup
  2. Sets up segment registers (SS = DS)
  3. Initializes stack (SP = 0, BP = 0)
  4. Re-enables interrupts
  5. Passes boot drive to C function (in DX)
  6. Calls C entry point `_cstart_`
  7. Halts if C function returns
- **Key concept:** Bridge between Stage 1 assembly and Stage 2 C code

**`src/bootloader/stage2/main.c`** (7 lines)
- **Purpose:** C entry point for Stage 2
- **Current functionality:**
  - Displays "Hello world from C!"
  - Infinite loop to prevent return
- **Parameters:** `uint16_t bootDrive` (passed from main.asm)
- **Future:** Will load kernel from filesystem

**`src/bootloader/stage2/stdio.h`** (6 lines)
- **Purpose:** Interface for screen output
- **Functions:**
  - `void putc(char c)` - Output single character
  - `void puts(const char* str)` - Output null-terminated string

**`src/bootloader/stage2/stdio.c`** (16 lines)
- **Purpose:** Implementation of stdio functions
- **How it works:**
  - `putc()` calls assembly wrapper for BIOS INT 10h
  - `puts()` loops through string calling `putc()` for each character
- **Note:** No buffering, writes directly to screen via BIOS

**`src/bootloader/stage2/stdint.h`** (13 lines)
- **Purpose:** Fixed-width integer type definitions
- **Why needed:** C standard library not available in bare-metal environment
- **Types defined:** int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t

**`src/bootloader/stage2/x86.h`** (6 lines)
- **Purpose:** Interface for x86 BIOS interrupt wrappers
- **Functions:**
  - `void _cdecl x86_Video_WriteCharTeletype(char c, uint8_t page)`
- **Future:** Will add more BIOS interrupt wrappers (disk, keyboard, etc.)

**`src/bootloader/stage2/x86.asm`** (22 lines)
- **Purpose:** Assembly wrappers for BIOS interrupts
- **Current function:** `_x86_Video_WriteCharTeletype`
  - Sets up stack frame
  - Gets parameters from stack (character at BP+4, page at BP+6)
  - Calls BIOS INT 10h, AH=0Eh (teletype output)
  - Cleans up and returns
- **Key concept:** Follows Open Watcom _cdecl calling convention

**`src/bootloader/stage2/linker.lnk`** (12 lines)
- **Purpose:** Linker script for creating raw binary from OBJ files
- **Configuration:**
  - Output format: Raw binary (not ELF or COFF)
  - Entry point: `entry` symbol (from main.asm)
  - Segment order: _ENTRY (asm), _TEXT (C), DATA
  - Load address: Offset 0 (Stage 1 sets segments)
  - Stack: 512 bytes reserved (though we set SS:SP manually)

**`src/bootloader/stage2/Makefile`** (33 lines)
- **Purpose:** Build rules for Stage 2 bootloader
- **What it does:**
  - Finds all .c and .asm files automatically
  - Compiles .c files with Open Watcom (wcc)
  - Assembles .asm files to OBJ format (nasm -f obj)
  - Links all OBJ files into single raw binary (wlink)
  - Creates stage2.map for debugging
- **Variables:** BUILD_DIR, ASM, CC16, LD16, ASMFLAGS, CFLAGS16

#### Kernel Files

**`src/kernel/main.asm`** (~50 lines)
- **Purpose:** Simple kernel for testing (not currently loaded)
- **What it does:**
  1. Sets up segment registers (DS, ES = 0x2000)
  2. Displays "Hello world from kernel!" message
  3. Halts the CPU
- **Load address:** Would be loaded at 0x2000:0x0000 by Stage 2 (when implemented)
- **Status:** Not used yet (Stage 2 doesn't load kernel yet)

**`src/kernel/Makefile`** (10 lines)
- **Purpose:** Build rules for kernel
- **What it does:** Assembles main.asm to kernel.bin
- **Status:** Can be built but not integrated into boot process yet

#### Build System Files

**`Makefile`** (73 lines, main build file)
- **Purpose:** Top-level build orchestration
- **Targets:**
  - `all` - Build floppy image and tools (default)
  - `floppy_image` - Create bootable 1.44 MB disk image
  - `bootloader` - Build Stage 1 and Stage 2
  - `stage1` - Build Stage 1 bootloader
  - `stage2` - Build Stage 2 bootloader
  - `tools_fat` - Build FAT12 utility
  - `clean` - Remove all build artifacts
  - `always` - Ensure build directory exists
- **Key steps:**
  1. Creates blank 1.44 MB image with dd
  2. Formats as FAT12 with mkfs.fat
  3. Installs Stage 1 as boot sector
  4. Copies Stage 2 and test files to filesystem

#### Utility Files

**`tools/fat/fat.c`** (~200 lines)
- **Purpose:** Utility to read files from FAT12 disk images
- **Usage:** `./build/tools/fat <disk image> <filename>`
- **What it does:**
  1. Reads boot sector to get filesystem parameters
  2. Reads File Allocation Table (FAT)
  3. Reads root directory
  4. Searches for specified file
  5. Follows cluster chain to read entire file
  6. Outputs file contents (printable characters or hex)
- **Example:** `./build/tools/fat build/main_floppy.img "TEST    TXT"`
- **Note:** Filename must be in 8.3 format with spaces

**`run.sh`** (15 lines)
- **Purpose:** Convenience script to build and run OS
- **What it does:**
  1. Runs `make` to build everything
  2. Checks if build succeeded
  3. Launches QEMU with floppy disk image
- **Usage:** `./run.sh`

**`test.sh`** (3 lines)
- **Purpose:** Test script (currently minimal)
- **Status:** Placeholder for future automated tests

**`test.txt`** (1 line)
- **Purpose:** Sample text file for testing FAT12 reading
- **Content:** "This is a test text file"
- **Usage:** Copied to disk image, can be read with FAT utility

#### Documentation

**`README.md`** (2300+ lines)
- **Purpose:** Complete documentation of the OS
- **Sections:**
  - Project overview and architecture
  - Detailed component breakdowns
  - Complete code explanations
  - Build system documentation
  - Usage instructions
  - Troubleshooting guide
  - Learning resources

**`LICENSE`** 
- **Purpose:** Software license terms

### Build Artifacts (Generated)

**`build/stage1.bin`** (512 bytes)
- Stage 1 bootloader binary
- First 512 bytes of disk image (boot sector)
- Contains FAT12 BPB header

**`build/stage2.bin`** (~34 bytes currently)
- Stage 2 bootloader binary
- Stored as file in FAT12 filesystem
- Will grow as features added

**`build/stage2.map`** (text file)
- Linker map file showing memory layout
- Lists all segments, their addresses, and sizes
- Useful for debugging

**`build/main_floppy.img`** (1,474,560 bytes = 1.44 MB)
- Bootable floppy disk image
- Contains FAT12 filesystem
- Files: STAGE2.BIN, TEST.TXT
- Can be booted in QEMU, VirtualBox, VMware, or real hardware

**`build/stage2/asm/*.obj`** (OBJ format)
- Intermediate object files from assembly
- main.obj - Entry point
- x86.obj - BIOS wrappers

**`build/stage2/c/*.obj`** (OBJ format)
- Intermediate object files from C
- main.obj - cstart_ function
- stdio.obj - putc and puts

**`build/tools/fat`** (executable)
- Compiled FAT12 utility program
- Native executable (not for OS, runs on host)

---

## Technical Details

### Current Project Status

**✅ Fully Implemented and Working:**
- ✅ Stage 1 bootloader (512 bytes, FAT12 filesystem support)
  - BIOS disk detection and geometry detection
  - FAT12 root directory parsing
  - File search and cluster chain following
  - Loads STAGE2.BIN from filesystem
  - Error handling with messages
- ✅ Stage 2 bootloader architecture
  - Assembly entry point with proper segment setup
  - C code integration via Open Watcom compiler
  - Stack initialization
  - Boot drive parameter passing
- ✅ C standard library subset
  - stdint.h (fixed-width integer types)
  - stdio.h interface (putc, puts)
  - stdio.c implementation
- ✅ BIOS interrupt wrappers
  - x86.h interface
  - x86.asm implementation (INT 10h teletype output)
  - Proper Open Watcom calling convention
- ✅ Build system
  - Modular Makefiles for each component
  - Automatic dependency building
  - Floppy image creation
  - FAT12 utility for testing
- ✅ FAT12 disk image utility
  - Can read files from disk images
  - Demonstrates FAT12 implementation in C

**✅ Verified Working:**
- ✅ Boots successfully in QEMU
- ✅ Boots successfully in VirtualBox
- ✅ Boots successfully in VMware
- ✅ Displays "Hello world from C!" on screen
- ✅ FAT12 filesystem reading works correctly
- ✅ Two-stage boot process works seamlessly

**🚧 Partially Implemented:**
- 🚧 Stage 2 bootloader functionality
  - ✅ Can display messages
  - ❌ Doesn't load kernel yet
  - ❌ No keyboard input handling
  - ❌ No advanced initialization
- 🚧 Error handling
  - ✅ Stage 1 has error messages
  - ❌ Stage 2 has no error handling
  - ❌ No diagnostic output

**❌ Not Yet Implemented:**
- ❌ Kernel loading from Stage 2
- ❌ Kernel.bin execution
- ❌ Protected mode transition
- ❌ Memory management (paging, heap)
- ❌ Interrupt Descriptor Table (IDT) setup
- ❌ Device drivers (keyboard, disk, etc.)
- ❌ Any actual OS functionality
- ❌ User mode vs kernel mode
- ❌ System calls
- ❌ Multitasking
- ❌ File system write support

### Next Steps (Priority Order)

**Immediate Next Steps (Stage 2 Enhancement):**
1. **Implement kernel loading in Stage 2**
   - Add FAT12 reading code to Stage 2 (C implementation)
   - Load KERNEL.BIN from filesystem
   - Transfer control to kernel at 0x2000:0x0000
   - Pass boot drive and memory map to kernel

2. **Add error handling to Stage 2**
   - Implement error message printing
   - Handle file not found errors
   - Handle disk read errors
   - Provide diagnostic information

3. **Memory detection**
   - Use BIOS INT 0x15, EAX=0xE820 to detect memory
   - Create memory map for kernel
   - Pass memory information to kernel

4. **Keyboard input in Stage 2**
   - Add BIOS INT 16h wrapper
   - Simple key waiting function
   - Boot menu functionality (optional)

**Short-term Goals (Basic Kernel):**
1. Create actual kernel.asm with more functionality
2. Implement VGA text mode driver (replace BIOS)
3. Setup Interrupt Descriptor Table (IDT)
4. Implement keyboard driver (PS/2)
5. Basic shell/command interpreter

**Medium-term Goals (Protected Mode):**
1. Setup Global Descriptor Table (GDT)
2. Enable A20 line
3. Transition to 32-bit protected mode
4. Rewrite drivers for protected mode
5. Implement paging
6. Basic memory allocator

**Long-term Goals (Real OS Features):**
1. System calls interface
2. User mode vs kernel mode separation
3. Process management
4. Simple filesystem (FAT16/32 or custom)
5. Multi-tasking (preemptive)
6. ELF executable loader

### Why FAT12?

FAT12 is ideal for learning OS development:

**Advantages:**
- Simple structure (easy to understand and implement)
- Well-documented specification
- Supported by all operating systems (can read/write on Windows, Linux, macOS)
- Small metadata overhead
- No complex structures (no journaling, no permissions)

**Limitations:**
- Maximum disk size: ~32 MB (4086 clusters × 8 KB/cluster)
- For floppies: Maximum 2847 clusters
- 12-bit addressing limits scalability
- No file permissions or timestamps (basic support only)
- Fragmentation can waste space

**Why not FAT16/FAT32?**
- More complex entry packing
- Larger metadata structures
- Unnecessary for floppy-sized media
- FAT12 teaches core concepts that apply to all FAT variants

### Real Mode vs Protected Mode

**Current: 16-bit Real Mode**

This OS currently runs in real mode (8086 compatible):

**Characteristics:**
- Segmented memory model: `address = segment × 16 + offset`
- 1 MB address space (20-bit addressing)
- Only 640 KB usable (upper 384 KB reserved for hardware)
- No memory protection (any code can access any memory)
- No privilege levels (all code runs in Ring 0)
- Direct hardware access via port I/O
- BIOS interrupts available

**Advantages for learning:**
- Simple to understand
- Direct hardware control
- No complex setup required
- BIOS provides device drivers
- Easy to debug with QEMU

**Disadvantages:**
- Severe memory limitations
- No memory protection (crashes can corrupt everything)
- No multitasking support
- 16-bit only (slow on modern CPUs)
- Security vulnerabilities

**Future: 32-bit Protected Mode**

Protected mode enables modern OS features:

**Characteristics:**
- Flat memory model: 4 GB address space
- Memory protection (segments can be read-only, execute-only, etc.)
- Privilege levels (Ring 0-3): kernel vs user code
- Virtual memory support (paging)
- Hardware task switching
- Interrupt Descriptor Table (IDT) replaces IVT

**Transition process:**
1. Disable interrupts
2. Load Global Descriptor Table (GDT)
3. Enable A20 line (access above 1 MB)
4. Set PE bit in CR0 register
5. Far jump to 32-bit code segment
6. Setup new segment registers
7. Setup IDT for protected mode interrupts
8. Re-enable interrupts

**Future: 64-bit Long Mode**

Ultimate goal for modern OS:

**Characteristics:**
- 64-bit registers and addressing
- Flat memory model (no segmentation)
- 48-bit virtual addresses (256 TB)
- NX bit (non-executable pages)
- Canonical addressing
- Required for modern CPUs

### Open Watcom Compiler Details

**Why Open Watcom?**

Open Watcom is one of the few compilers still supporting 16-bit targets:

**Alternatives comparison:**
| Compiler | Status | 16-bit Support | License |
|----------|--------|----------------|---------|
| Open Watcom | Active (2024) | ✅ Excellent | Open Source (Sybase License) |
| Turbo C | Obsolete (1990s) | ✅ Native | Proprietary (freeware) |
| Borland C | Obsolete (2000s) | ✅ Native | Proprietary |
| GCC | Active | ⚠️ Limited (ia16-elf-gcc fork) | GPL |
| Clang | Active | ❌ No support | Apache 2.0 |

**Open Watcom advantages:**
- Modern build system (Make compatible)
- Good optimizer (produces compact code)
- Comprehensive documentation
- Cross-platform (Linux, Windows, macOS)
- Active community
- C and C++ support
- Inline assembly support

**Compiler optimizations:**

```bash
-ox    # Maximum optimization
-os    # Optimize for size
-ot    # Optimize for speed
-ol+   # Loop optimizations
-oi+   # Inline intrinsics
```

Current flags (`-4 -d3 -s -wx -ms -zl -zq`):
- `-4`: 80386 instructions (backwards compatible to 80286/8086)
- `-d3`: Full debugging info (line numbers, variable names)
- `-s`: Disable stack overflow checking (saves ~20 bytes per function)
- `-wx`: Maximum warnings (helps catch errors)
- `-ms`: Small memory model
- `-zl`: No default library (we provide our own functions)
- `-zq`: Quiet (suppress banner)

### Linker Script Analysis

The `linker.lnk` file controls Stage 2's memory layout:

```text
OPTION NODEFAULTLIBS     # Don't link C runtime
OPTION NOINIT            # Don't run C initialization code
ORDER                    # Segment order in output file
    CLNAME CODE SEGMENT _ENTRY    # Assembly entry (first)
    CLNAME CODE SEGMENT _TEXT      # C code (second)
    CLNAME DATA SEGMENT CONST      # Read-only data
    CLNAME DATA SEGMENT CONST2     # More read-only data
    CLNAME DATA SEGMENT _DATA      # Initialized variables
OPTION OFFSET=0          # Start at offset 0
OPTION START=entry       # Entry point symbol
FORMAT RAW BIN           # Raw binary output
```

**Why this order?**
1. `_ENTRY` must be first (Stage 1 jumps to byte 0)
2. Code sections before data (standard practice)
3. Predictable layout for debugging

**Alternative: OMF EXE format**
```text
FORMAT DOS              # MS-DOS EXE format
OPTION STACK=2048      # Stack size
OPTION START=_main     # C main() function
LIBPATH %WATCOM%/lib286  # Link with C runtime
```

This would create a .COM or .EXE file, but we need raw binary.

### BIOS Interrupts Used

**INT 10h - Video Services**
- `AH=0x00`: Set video mode
- `AH=0x0E`: Teletype output (used in puts)
- `AH=0x13`: Write string

**INT 13h - Disk Services**
- `AH=0x00`: Reset disk system
- `AH=0x02`: Read sectors (used extensively)
- `AH=0x08`: Get drive parameters
- `AH=0x15`: Get disk type

**INT 16h - Keyboard Services**
- `AH=0x00`: Wait for keypress (used in error handling)
- `AH=0x01`: Check for keypress

**Why use BIOS?**
- Pre-written device drivers
- Works on all hardware
- Simple interface
- No driver development needed

**Disadvantages:**
- Only available in real mode
- Slow (mode switches on modern CPUs)
- Limited functionality
- Must implement own drivers for protected mode

### Debugging Techniques

**QEMU debugging:**
```bash
# Start QEMU with GDB stub
qemu-system-i386 -fda build/main_floppy.img -s -S

# In another terminal, start GDB
gdb
(gdb) target remote localhost:1234
(gdb) set architecture i8086
(gdb) break *0x7c00        # Break at bootloader start
(gdb) continue
(gdb) x/32xb 0x7c00        # Examine memory
(gdb) info registers
```

**QEMU monitor:**
```bash
# Press Ctrl+Alt+2 in QEMU window
(qemu) info registers       # Show CPU state
(qemu) info mem            # Show memory map
(qemu) x /10i $pc          # Disassemble at PC
(qemu) xp /32x 0x7c00      # Dump memory (physical)
```

**Bochs debugging:**
```bash
# Install Bochs with debugger
sudo apt install bochs bochs-x

# Create bochsrc.txt
floppya: 1_44=build/main_floppy.img, status=inserted
boot: floppy
display_library: x, options="gui_debug"

# Run
bochs -f bochsrc.txt -q
```

**Adding debug output:**
```c
void debug_byte(uint8_t value) {
    putc((value >> 4) < 10 ? '0' + (value >> 4) : 'A' + (value >> 4) - 10);
    putc((value & 0xF) < 10 ? '0' + (value & 0xF) : 'A' + (value & 0xF) - 10);
}
```

### Common Issues and Solutions

**Build Issues:**

**Issue:** `wcc: No such file or directory` or `wlink: command not found`
- **Cause:** Open Watcom is not installed or not in PATH
- **Solution:** 
  ```bash
  # Download from https://open-watcom.github.io/
  # Install to /usr/local/watcom or your preferred location
  export WATCOM=/usr/local/watcom
  export PATH=$WATCOM/binl64:$PATH
  # Add to ~/.bashrc to make permanent
  ```
- **Verify installation:**
  ```bash
  wcc -?
  wlink -?
  ```

**Issue:** `nasm: command not found`
- **Solution:** 
  ```bash
  # Ubuntu/Debian:
  sudo apt install nasm
  
  # Fedora/RHEL:
  sudo dnf install nasm
  
  # macOS:
  brew install nasm
  ```

**Issue:** `mkfs.fat: command not found` or `mcopy: command not found`
- **Solution:**
  ```bash
  # Ubuntu/Debian:
  sudo apt install dosfstools mtools
  
  # Fedora/RHEL:
  sudo dnf install dosfstools mtools
  ```

**Issue:** `Error! E2028: _x86_Video_WriteCharTeletype is an undefined reference`
- **Cause:** Mismatch between C function declaration and assembly implementation
- **Solution:** 
  - Check assembly file has `global _x86_Video_WriteCharTeletype` (with underscore prefix)
  - Verify C header declares function with `_cdecl` attribute
  - Ensure x86.asm is being assembled and linked

**Issue:** `Warning! W1014: stack segment not found`
- **Cause:** Normal for raw binary output (we manually set SS:SP)
- **Solution:** This warning is safe to ignore - we set up the stack manually in main.asm

**Issue:** Build succeeds but `stage1.bin` is not 512 bytes
- **Cause:** Boot sector assembly is incorrect
- **Solution:** 
  ```bash
  ls -l build/stage1.bin
  # Must be exactly 512 bytes
  ```
  - Check `times 510-($-$$) db 0` padding in boot.asm
  - Verify boot signature `dw 0AA55h` is present

**Runtime Issues:**

**Issue:** QEMU shows blank screen / nothing happens
- **Cause:** Bootloader not installed correctly or boot signature missing
- **Solution:**
  ```bash
  # Check boot signature
  hexdump -C build/main_floppy.img | head -n 32
  # Bytes 510-511 (last 2 bytes of sector 0) should be: 55 aa
  
  # Rebuild from clean state
  make clean
  make
  ```

**Issue:** `STAGE2.BIN not found!` displayed on screen
- **Cause:** Stage 2 wasn't copied to disk image or has wrong filename
- **Solution:**
  ```bash
  # Check if file exists in image
  mdir -i build/main_floppy.img ::/
  # Should show STAGE2   BIN
  
  # If missing, rebuild
  make clean
  make
  ```
- **Note:** Filename must be exactly "STAGE2  BIN" (with spaces, 8.3 format)

**Issue:** System displays "Hello world from C!" then immediately reboots or crashes
- **Cause:** This is actually SUCCESS! The system is working correctly.
- **What's happening:** Stage 2's main.c has `for(;;);` infinite loop to halt after displaying message
- **If it crashes:** Check that the infinite loop is present in main.c

**Issue:** Garbage characters displayed instead of message
- **Cause:** String is not null-terminated or segment registers incorrect
- **Solution:**
  - Verify string in main.c ends with `!` (puts expects null-terminated string)
  - Check that DS points to correct segment in main.asm
  - Use QEMU debugger to inspect memory

**Issue:** `qemu-system-i386: -fda: drive with bus=0, unit=0 (index=0) exists`
- **Cause:** Another QEMU instance is using the disk image
- **Solution:**
  ```bash
  # Kill other QEMU instances
  pkill qemu
  # Or use a different invocation
  qemu-system-i386 -drive file=build/main_floppy.img,if=floppy,format=raw
  ```

**Issue:** Stage 2 crashes immediately (QEMU resets or triple faults)
- **Cause:** Entry point not at offset 0, or stack setup incorrect
- **Solution:**
  ```bash
  # Check linker map
  cat build/stage2.map | grep "_ENTRY"
  # _ENTRY segment should start at address 00000000
  
  # Verify stage2.bin starts with correct code
  hexdump -C build/stage2.bin | head -n 5
  # Should start with: fa (cli instruction)
  ```

**Issue:** C function parameters are wrong or corrupted
- **Cause:** Stack frame layout doesn't match Open Watcom calling convention
- **Solution:**
  - Verify parameters accessed at [bp + 4], [bp + 6], etc. in assembly
  - Check that BP is set up correctly: `push bp; mov bp, sp`
  - Ensure parameters pushed in correct order (right-to-left)

**Debugging Issues:**

**Issue:** Want to see what's happening during boot
- **Solution:** Use QEMU monitor and debugger:
  ```bash
  # Start QEMU with debugging enabled
  qemu-system-i386 -fda build/main_floppy.img -d int,cpu_reset -D qemu_log.txt
  
  # In another terminal, use GDB
  gdb
  (gdb) target remote localhost:1234
  (gdb) set architecture i8086
  (gdb) break *0x7c00
  (gdb) continue
  ```

**Issue:** Want to inspect disk image filesystem
- **Solution:**
  ```bash
  # List all files
  mdir -i build/main_floppy.img ::/
  
  # View file contents
  mtype -i build/main_floppy.img ::/test.txt
  
  # Copy file out of image
  mcopy -i build/main_floppy.img ::/stage2.bin ./stage2_extracted.bin
  
  # Get detailed info
  minfo -i build/main_floppy.img
  ```

**Performance/Testing Issues:**

**Issue:** Want to test on real hardware
- **Solution:** Install Open Watcom and add to PATH
  ```bash
  export WATCOM=/usr/local/watcom
  export PATH=$WATCOM/binl64:$PATH
  ```

**Issue:** `Error! E2028: symbol is an undefined reference`
- **Solution:** Check function name mangling (C adds `_` prefix)
- Verify `global _function_name` in assembly
- Ensure `extern` declaration in C header

**Issue:** `Warning! W1014: stack segment not found`
- **Solution:** This is normal for raw binaries (we set SS:SP manually)
- Safe to ignore

**Issue:** `STAGE2.BIN not found!` when booting
- **Solution:** Verify Stage 2 was copied to image
  ```bash
  mdir -i build/main_floppy.img ::/
  ```
- Check filename (must be `STAGE2  BIN` with spaces)

**Issue:** Stage 2 crashes immediately
- **Solution:** Check linker map file (`stage2.map`)
- Verify entry point is at offset 0
- Check segment initialization in main.asm
- Use QEMU debugger to inspect

**Issue:** Garbage characters on screen
- **Solution:** Check string null terminator
- Verify correct segment registers (DS should point to string data)
- Ensure `_TEXT` segment is correctly loaded

**Issue:** Parameters passed to C function are wrong
- **Solution:** Verify stack frame layout
- Check Open Watcom calling convention (parameters at BP+4, BP+6, ...)
- Ensure parameter types match (uint8_t is 8-bit, uint16_t is 16-bit)

### Performance Considerations

**Stage 1 Bootloader:**
- Loading time: ~2 seconds on real hardware (floppy seeks are slow)
- Code size: 512 bytes (fully utilized)
- Memory usage: ~30 KB (FAT + root directory buffers)

**Stage 2 Bootloader:**
- Current size: ~34 bytes (minimal test code)
- Maximum practical size: ~29 KB (before hitting Stage 1 at 0x7C00)
- Load time: ~1 second for 10 KB file
- C code overhead: ~200 bytes (startup code, stack setup)

**Optimization tips:**
- Use `-os` for size optimization
- Inline small functions manually
- Avoid large local variables (use globals)
- Use `register` keyword for frequently used variables
- Unroll small loops
- Use bitwise operations instead of multiply/divide

**Size comparison:**
```text
Component         Size      Percentage
=====================================
Stage 1          512 B      100% (fixed)
Stage 2 (asm)     19 B       56%
Stage 2 (C)       15 B       44%
Total Stage 2     34 B      100%
```

Stage 2 will grow significantly as features are added (target: ~10-20 KB).

---

## Future Enhancement Ideas

**Short term (next steps):**
- ✅ Display message from Stage 2 C code
- 🔄 Load kernel from Stage 2
- 🔄 Enhanced error messages
- ⬜ Keyboard input handling
- ⬜ Basic memory detection

**Medium term:**
- ⬜ Transition to 32-bit protected mode
- ⬜ Setup GDT and IDT
- ⬜ Implement basic paging
- ⬜ Simple memory allocator (heap)
- ⬜ VGA text mode driver (replace BIOS)

**Long term:**
- ⬜ Multi-tasking (cooperative then preemptive)
- ⬜ System calls (INT 0x80 or SYSCALL)
- ⬜ User mode vs kernel mode
- ⬜ ELF executable loader
- ⬜ Simple shell
- ⬜ Virtual File System (VFS)
- ⬜ Block device abstraction
- ⬜ FAT16/32 support
- ⬜ PS/2 keyboard driver
- ⬜ PIC (Programmable Interrupt Controller) driver
- ⬜ PIT (Programmable Interval Timer) driver

**Advanced features:**
- ⬜ VESA graphics mode
- ⬜ Networking (NE2000 or RTL8139 driver)
- ⬜ ATA/ATAPI disk driver
- ⬜ Simple GUI (windowing system)
- ⬜ Transition to 64-bit long mode

---

## Learning Resources

**Essential reading:**
- [OSDev Wiki](https://wiki.osdev.org/) - Comprehensive OS development resource
- [Intel 80386 Programmer's Reference](https://pdos.csail.mit.edu/6.828/2018/readings/i386.pdf) - CPU architecture
- [BIOS Interrupt List](http://www.ctyme.com/intr/int.htm) - Complete interrupt reference
- [FAT Filesystem Specification](https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/fatgen103.doc) - Microsoft's official spec

**Tutorials:**
- [Writing a Simple Operating System from Scratch](https://www.cs.bham.ac.uk/~exr/lectures/opsys/10_11/lectures/os-dev.pdf) - Excellent intro
- [Bran's Kernel Development Tutorial](http://www.osdever.net/bran/kdev/) - Classic tutorial
- [JamesM's Kernel Development Tutorials](http://www.jamesmolloy.co.uk/tutorial_html/) - Modern approach

**Books:**
- "Operating Systems: Design and Implementation" by Andrew Tanenbaum (MINIX)
- "Operating System Concepts" by Silberschatz, Galvin, Gagne
- "Modern Operating Systems" by Andrew Tanenbaum

**Tools documentation:**
- [Open Watcom Documentation](http://openwatcom.org/doc.php)
- [NASM Manual](https://www.nasm.us/xdoc/2.15.05/html/nasmdoc0.html)
- [QEMU Documentation](https://www.qemu.org/docs/master/)

**Community:**
- [OSDev Forum](https://forum.osdev.org/) - Active community
- [r/osdev](https://www.reddit.com/r/osdev/) - Reddit community
- [OSDev Discord](https://discord.gg/RnCtsqD) - Real-time chat

---

## License

See LICENSE file for details.

---

## Acknowledgments

This project was built by following various online tutorials and resources, with significant enhancements and documentation. The journey from a single-stage bootloader to a two-stage C-based bootloader demonstrates the evolution of understanding and skill development in OS programming.

**Key learning milestones:**
1. Understanding BIOS boot process and boot sector constraints
2. Implementing FAT12 filesystem parsing
3. Managing memory in real mode
4. Integrating C code with assembly
5. Using Open Watcom for bare-metal development
6. Creating modular build systems
7. Debugging low-level code

The experience gained includes:
- Low-level programming in x86 assembly
- Bare-metal C programming (no standard library)
- Filesystem implementation
- Hardware interfacing (BIOS interrupts)
- Bootloader development
- Build system design
- Cross-platform development
- Debugging techniques for system software

This project serves as an educational foundation for understanding:
- How computers boot
- How operating systems manage hardware
- How filesystems organize data
- How compilers and linkers work
- The interface between software and hardware
- Memory management in constrained environments

**Current status:** Successfully boots in QEMU, displays "Hello world from C!_" from Stage 2 bootloader. Ready for next phase: kernel loading and protected mode transition.

Happy OS development! 🚀

---

*Last updated: November 14, 2025*
*Project phase: Two-stage bootloader with C integration (complete)*
*Next milestone: Kernel loading and protected mode*
- **Solution:** Ensure kernel.bin was copied correctly. Check with:
  ```bash
  mdir -i build/main_floppy.img ::/
  ```

**Issue:** Bootloader hangs or resets
- **Solution:** Check boot signature (last 2 bytes must be 0xAA55)

**Issue:** Want to test on real hardware
- **Solution:**
  ```bash
  # CAUTION: This will erase the USB drive!
  # Find USB device
  lsblk
  # Write image (replace /dev/sdX with your USB device)
  sudo dd if=build/main_floppy.img of=/dev/sdX bs=512
  sudo sync
  # Boot from USB on real computer
  ```
- **Note:** Image is only 1.44 MB, rest of USB will be unused
- **Warning:** Some modern BIOS might not boot from such small images

**Issue:** Build is slow
- **Current build time:** ~1-2 seconds on modern hardware
- **If slower:** Check if antivirus is scanning build directory
- **Optimization:** Build artifacts are minimal, so clean builds are fast

### Limitations and Known Issues

**Current Limitations:**

1. **No Kernel Loading Yet** 
   - Stage 2 displays message but doesn't load kernel
   - Kernel loading code needs to be implemented in Stage 2 C code
   - Currently just has `for(;;);` infinite loop after displaying message

2. **64 KB Segment Limits**
   - Stage 2 limited to ~29 KB before hitting Stage 1 at 0x7C00
   - Future kernel would be limited to 64 KB per segment in real mode
   - Solution: Transition to protected mode for larger programs

3. **No Error Recovery in Stage 2**
   - If Stage 2 encounters an error, it just halts
   - No error messages implemented yet
   - Stage 1 has error messages, but Stage 2 doesn't

4. **No User Input**
   - System doesn't handle keyboard input
   - Only outputs to screen via BIOS INT 10h
   - Keyboard interrupt handlers not implemented

5. **Single-tasking Only**
   - No multitasking support
   - No process/thread management
   - Runs in single execution context

6. **Real Mode Limitations**
   - Only 1 MB addressable memory (640 KB usable)
   - No memory protection
   - No virtual memory
   - Direct hardware access (can corrupt system easily)

7. **FAT12 Limitations**
   - Read-only (can't write files)
   - Maximum 2847 clusters on floppy
   - Files up to ~1.4 MB maximum
   - 8.3 filename format only

**Known Issues/Quirks:**

1. **Linker Warning About Stack**
   - Warning: "stack segment not found"
   - This is NORMAL and expected for raw binary output
   - We manually set SS:SP in assembly, so no stack segment needed

2. **Stage 2 Size Reporting**
   - Linker may report different sizes depending on options
   - Actual binary size is what matters (currently ~34 bytes)
   - Future additions will increase size significantly

3. **Boot Drive Number**
   - DL register contains boot drive from BIOS
   - Stage 1 passes this to Stage 2
   - Stage 2 currently doesn't use it (but receives it as parameter)

4. **Message Display**
   - Message appears at current cursor position
   - No cursor positioning implemented
   - No screen clearing implemented
   - Just uses BIOS teletype function (scrolls if needed)

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
