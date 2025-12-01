# project_OS - A Custom Operating System from Scratch

My journey of scouring the depths of the internet to learn and build my very own OS (hopefully)

## Quick Start

**To build and run:**
```bash
./run.sh
```

**Expected output:**
```
==================================================
          KERNEL STARTED SUCCESSFULLY
==================================================

[INFO] Kernel loaded and running in 32-bit protected mode
[INFO] VGA text mode initialized (80x25)
[INFO] Video memory at 0xB8000

[OK] System initialization complete

--------------------------------------------------
System Details:
  - Architecture: x86 (i386)
  - Mode: 32-bit Protected Mode
  - Kernel Address: 0x100000 (1MB)
  - Version: 1.0.0
  - Build Date: 2025-11-30
--------------------------------------------------

Hello world from kernel!

Kernel is now idle. Press Ctrl+Alt+Del to reboot.
```

**What this demonstrates:**
- ✅ Two-stage bootloader working
- ✅ FAT12 filesystem reading operational  
- ✅ C code executing in 16-bit real mode
- ✅ BIOS interrupts accessible from C
- ✅ Kernel loading and execution
- ✅ Enhanced logging with log levels (INFO, OK, ERROR, DEBUG)
- ✅ VGA text mode with color support in kernel

**Current status:** Stage 1 and Stage 2 bootloaders complete. Kernel loading implemented with enhanced logging.

## Table of Contents
1. [Project Overview](#project-overview)
2. [Operating System Fundamentals: Theory vs Implementation](#operating-system-fundamentals-theory-vs-implementation)
   - [Boot Process: How OSes Start](#boot-process-how-oses-start)
   - [Memory Management Approaches](#memory-management-approaches)
   - [Filesystem Design Choices](#filesystem-design-choices)
   - [CPU Modes and Privilege Levels](#cpu-modes-and-privilege-levels)
   - [Interrupt Handling Strategies](#interrupt-handling-strategies)
3. [System Architecture](#system-architecture)
4. [Visual Architecture and Data Flow](#visual-architecture-and-data-flow)
   - [Complete System Boot Flow Diagram](#complete-system-boot-flow-diagram)
   - [Memory Layout During Boot Process](#memory-layout-during-boot-process)
   - [FAT12 Filesystem Structure](#fat12-filesystem-structure)
   - [Protected Mode Transition](#protected-mode-transition)
   - [Build System Data Flow](#build-system-data-flow)
5. [Detailed Component Breakdown](#detailed-component-breakdown)
   - [Stage 1 Bootloader (boot.asm)](#stage-1-bootloader-bootasm)
   - [Stage 2 Bootloader (main.asm + main.c)](#stage-2-bootloader-mainasm--mainc)
   - [C Standard Library Components](#c-standard-library-components)
   - [x86 Assembly Helpers](#x86-assembly-helpers)
   - [Kernel (entry.asm + main.c)](#kernel-entryasm--mainc)
   - [FAT12 Utility (fat.c)](#fat12-utility-fatc)
   - [Build System (Makefile)](#build-system-makefile)
6. [Toolchain and Build System Design](#toolchain-and-build-system-design)
   - [Compiler and Assembler Choices](#compiler-and-assembler-choices)
   - [Build System Architecture](#build-system-architecture)
   - [Testing and Debugging Infrastructure](#testing-and-debugging-infrastructure)
7. [How Everything Works Together](#how-everything-works-together)
8. [Building and Running](#building-and-running)
9. [Technical Details](#technical-details)
10. [Implementation Deep-Dive: Code Walkthrough](#implementation-deep-dive-code-walkthrough)
    - [Stage 1 Bootloader: Complete Code Analysis](#stage-1-bootloader-complete-code-analysis)
    - [Stage 2 Bootloader: C/Assembly Integration](#stage-2-bootloader-cassembly-integration)
    - [Kernel: 64-bit Long Mode Implementation](#kernel-64-bit-long-mode-implementation)
    - [Interrupt Handling Implementation](#interrupt-handling-implementation)
    - [Keyboard Driver Implementation](#keyboard-driver-implementation)
    - [VGA Text Mode Implementation](#vga-text-mode-implementation)
    - [FAT12 Implementation Details](#fat12-implementation-details)
    - [Shell Implementation](#shell-implementation)
11. [Future Enhancement Ideas](#future-enhancement-ideas)
12. [How to Extend This OS](#how-to-extend-this-os)
13. [Design Philosophy and Lessons Learned](#design-philosophy-and-lessons-learned)

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

## Operating System Fundamentals: Theory vs Implementation

This section provides a deep dive into how operating systems work in general, how this OS implements these concepts, and what alternative approaches exist.

### Boot Process: How OSes Start

#### General OS Boot Theory

All operating systems must bootstrap themselves from power-on to a running state. This process is standardized on x86 systems:

**The Universal Boot Sequence:**

1. **Power-On Self Test (POST)**: Hardware initialization by firmware
2. **Firmware Stage**: BIOS/UEFI locates boot device
3. **Boot Sector Loading**: First sector loaded at fixed memory location
4. **Bootloader Execution**: Minimal code that loads the full bootloader
5. **OS Loader**: Full-featured loader that prepares the OS kernel
6. **Kernel Initialization**: OS kernel starts, initializes subsystems
7. **Init Process**: First user-space process starts (systemd, init, etc.)

**Why Multiple Stages?**

Operating systems universally use multi-stage bootloaders due to space constraints:

- **BIOS Boot Sector**: 512 bytes (446 bytes code + 64 bytes partition table + 2 bytes signature)
- **UEFI Boot**: Uses FAT32 filesystem, allows larger bootloaders
- **Physical Limitations**: Early PC hardware couldn't load more than 512 bytes initially

#### Our Implementation: Two-Stage Bootloader

**Stage 1 - Minimal FAT12 Loader (512 bytes)**

Located in `src/bootloader/stage1/boot.asm`, this is our equivalent to the Master Boot Record (MBR):

```assembly
org 0x7C00              ; BIOS loads us here
bits 16                 ; 16-bit real mode

; FAT12 BPB (BIOS Parameter Block)
; Must appear at specific offsets for filesystem compatibility
```

**What Stage 1 Does:**
1. Initialize segment registers (CS, DS, ES, SS)
2. Set up stack pointer at 0x7C00 (grows downward)
3. Detect disk geometry using BIOS INT 13h, AH=08h
4. Parse FAT12 filesystem structures:
   - Boot sector → FAT tables → Root directory → Data area
5. Search for `STAGE2.BIN` in root directory
6. Follow FAT12 cluster chain to load entire file
7. Load Stage 2 at 0x0500 (safe memory region)
8. Jump to Stage 2 entry point

**Why 0x0500?**
- 0x0000-0x04FF: BIOS Data Area (BDA) and Interrupt Vector Table (IVT)
- 0x0500-0x7BFF: Free conventional memory (~30 KB available)
- 0x7C00-0x7DFF: Our Stage 1 bootloader
- 0x7E00-0x9FFFF: More free memory

**Stage 2 - Full-Featured C Bootloader (Unlimited Size)**

Located in `src/bootloader/stage2/`, this stage is written in C with assembly glue code:

```c
// main.c entry point
void _cdecl cstart_(uint16_t bootDrive)
{
    // Initialize disk subsystem
    DISK_Initialize(bootDrive);
    
    // Initialize FAT filesystem
    if (!FAT_Initialize(&g_Disk)) {
        // Error handling
    }
    
    // Load kernel.bin from filesystem
    FAR_PTR kernelBuffer = FAT_Open(&g_RootDir, "/kernel.bin");
    
    // Switch to protected mode
    i386_EnterProtectedMode();
    
    // Jump to kernel at 1MB mark (0x100000)
    void (*kernelStart)() = (void(*)())0x100000;
    kernelStart();
}
```

**What Stage 2 Does:**
1. Re-initialize segment registers for C environment
2. Set up proper stack frame for C function calls
3. Initialize disk I/O subsystem with BIOS INT 13h wrappers
4. Initialize FAT12 filesystem driver
5. Load kernel.bin from filesystem
6. Prepare for protected mode:
   - Enable A20 line (access memory above 1MB)
   - Set up Global Descriptor Table (GDT)
   - Configure segment descriptors
7. Switch CPU to 32-bit protected mode
8. Jump to kernel entry point at 1MB

#### Alternative Boot Approaches

**1. Single-Stage Bootloader**

*How it works:* Everything in 512 bytes
- **Pros**: Simpler, no multi-stage complexity
- **Cons**: Extremely limited (no complex filesystems, minimal error handling)
- **Used by**: Very simple embedded systems, DOS boot sector viruses
- **Our choice**: Rejected due to feature limitations

**2. Three+ Stage Bootloader**

*How it works:* Stage 1 → Stage 1.5 → Stage 2 → Kernel
- **Pros**: More modular, stage 1.5 can fit between MBR and first partition
- **Cons**: More complexity, more loading time
- **Used by**: GRUB Legacy (had stage 1.5 for filesystem drivers)
- **Our choice**: Unnecessary complexity for learning project

**3. UEFI Boot**

*How it works:* Firmware loads .efi executable from FAT32 ESP (EFI System Partition)
- **Pros**: 
  - No 512-byte limit
  - Built-in filesystem support
  - Graphics before OS loads
  - Secure Boot support
- **Cons**:
  - More complex API
  - Requires UEFI firmware (not available on all hardware/emulators)
  - PE/COFF executable format required
- **Used by**: Modern operating systems (Windows 8+, modern Linux)
- **Our choice**: BIOS mode chosen for broader compatibility and simpler learning

**4. Network Boot (PXE)**

*How it works:* Firmware loads bootloader over network
- **Pros**: No local storage needed, centralized OS management
- **Cons**: Requires network infrastructure, slower boot
- **Used by**: Diskless workstations, cloud VMs, PXE boot environments
- **Our choice**: Not applicable for a learning OS project

**Comparison Table:**

| Approach | Complexity | Size Limit | Hardware Support | Our Use |
|----------|------------|------------|------------------|---------|
| Single-stage | Low | 446 bytes | Universal | ❌ Too limited |
| Two-stage (Ours) | Medium | Stage 2 unlimited | Universal | ✅ **Chosen** |
| Three-stage | High | Each stage unlimited | Universal | ❌ Unnecessary |
| UEFI | Very High | No limit | Modern systems only | ❌ Too complex |
| PXE | High | No limit | Network required | ❌ Not applicable |

---

### Memory Management Approaches

#### General Memory Management Theory

Operating systems must manage physical memory and provide abstractions for programs. Key concepts:

**1. Segmentation**

Memory divided into logical segments (code, data, stack):
- Each segment has base address and limit
- Provides basic protection (code vs data separation)
- Used heavily in x86 real mode and protected mode

**2. Paging**

Memory divided into fixed-size pages (typically 4KB):
- Virtual addresses mapped to physical addresses
- Enables virtual memory (swap to disk)
- Provides fine-grained protection
- Allows memory isolation between processes

**3. Memory Protection Rings**

x86 defines 4 privilege levels (rings 0-3):
- **Ring 0**: Kernel mode (full hardware access)
- **Ring 1-2**: Device drivers (rarely used)
- **Ring 3**: User mode (restricted access)

#### Our Implementation

**Real Mode (16-bit) - Bootloader**

In real mode, memory is accessed using segment:offset addressing:

```
Physical Address = (Segment × 16) + Offset
```

Example:
```
0x07C0:0x0000 = 0x07C00 (bootloader location)
0x0000:0x0500 = 0x00500 (stage 2 location)
0x2000:0x0000 = 0x20000 (old kernel location)
```

**Our Memory Map in Real Mode:**

```text
0x00000-0x003FF   1 KB     Interrupt Vector Table (IVT)
0x00400-0x004FF   256 B    BIOS Data Area (BDA)
0x00500-0x07BFF   ~30 KB   Stage 2 bootloader (loaded here)
0x07C00-0x07DFF   512 B    Stage 1 bootloader (BIOS loads here)
0x07E00-0x9FFFF   ~600 KB  Free conventional memory
0xA0000-0xBFFFF   128 KB   Video memory (VGA)
0xC0000-0xFFFFF   256 KB   ROM BIOS and adapters
```

**Why These Addresses?**

- **0x7C00**: IBM PC BIOS standard from 1981 (historical, provides 30KB stack below)
- **0x0500**: First safe address after BDA, allows ~30KB for Stage 2
- **0x100000**: 1MB mark, first address accessible in protected mode without A20 issues

**Protected Mode (32-bit) - Kernel**

Our kernel runs in 32-bit protected mode with flat memory model:

```c
// entry.asm - Kernel entry point
bits 32
_start:
    mov esp, 0x200000    ; Stack at 2MB mark
    mov ebp, esp
    call start           ; Jump to C kernel
```

**Protected Mode Memory Map:**

```text
0x00000000-0x000FFFFF   1 MB      Real mode area (legacy)
0x00100000-0x001FFFFF   1 MB      Kernel code and data
0x00200000-0x003FFFFF   2 MB      Kernel stack
0x00400000+             Rest      Future: User programs, heap, etc.
```

**GDT (Global Descriptor Table) Configuration:**

We set up a simple flat memory model:

```c
// Simplified GDT setup in stage2/main.c
struct GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;       // Present, Ring 0/3, Code/Data
    uint8_t granularity;  // 4KB pages, 32-bit
    uint8_t base_high;
} __attribute__((packed));

GDTEntry gdt[3] = {
    {0, 0, 0, 0x00, 0x00, 0},         // Null descriptor (required)
    {0xFFFF, 0, 0, 0x9A, 0xCF, 0},   // Code segment (0x08)
    {0xFFFF, 0, 0, 0x92, 0xCF, 0}    // Data segment (0x10)
};
```

**Segment Descriptors Explained:**

- **Limit**: 0xFFFF with 4KB granularity = 4GB (entire 32-bit address space)
- **Base**: 0x00000000 (flat model starts at 0)
- **Access byte (0x9A for code)**:
  - Present bit = 1 (segment is valid)
  - Privilege level = 00 (ring 0, kernel mode)
  - Descriptor type = 1 (code/data segment)
  - Executable = 1, Readable = 1
- **Granularity (0xCF)**:
  - Granularity = 1 (4KB pages)
  - Size = 1 (32-bit mode)
  - Limit high = 0xF

#### Alternative Memory Management Approaches

**1. Single Address Space OS**

*How it works:* All processes share same address space
- **Pros**: Fast context switching, easy IPC
- **Cons**: No memory protection, one crash kills everything
- **Used by**: Early Mac OS, AmigaOS, some embedded RTOS
- **Our choice**: Too dangerous for learning, no protection

**2. Segmented Memory Model**

*How it works:* Programs use logical segments (code, data, stack, heap)
- **Pros**: Natural program organization, coarse-grained protection
- **Cons**: Fragmentation, complex to manage, limited by segment size
- **Used by**: x86 real mode, OS/2, early Windows
- **Our choice**: Used in real mode (required), but transitioning away

**3. Flat Memory with Paging (Modern Approach)**

*How it works:* Single 4GB virtual address space per process, paged to physical RAM
- **Pros**: 
  - Simple programming model
  - Fine-grained protection (4KB pages)
  - Virtual memory support (swap)
  - Memory isolation between processes
- **Cons**: 
  - More complex to implement
  - TLB misses cost performance
  - Requires hardware support (MMU)
- **Used by**: Linux, Windows NT+, macOS, modern BSDs
- **Our future plan**: Will implement paging after protected mode is stable

**4. 64-bit Virtual Memory**

*How it works:* 48-bit virtual addresses (256TB address space)
- **Pros**: Massive address space, NX bit, simpler than PAE
- **Cons**: Requires 64-bit CPU, more complex page tables
- **Used by**: Modern Linux, Windows 10+, all modern desktop OSes
- **Our future plan**: Long-term goal after 32-bit is working

**Memory Model Comparison:**

| Model | Address Space | Protection | Complexity | Performance | Our Use |
|-------|---------------|------------|------------|-------------|---------|
| Real Mode (segmented) | 1 MB | None | Low | Fast | ✅ Bootloader |
| Protected Mode (flat) | 4 GB | Basic | Medium | Fast | ✅ Current kernel |
| Protected Mode (paging) | 4 GB virtual | Fine-grained | High | Medium | 🔄 Future |
| 64-bit Long Mode | 256 TB | Fine-grained + NX | Very High | Medium | ⭐ Long-term |

**Our Rationale:**

We start with segmentation (required in real mode), move to flat protected mode (simpler), and will eventually implement paging (necessary for real OS features).

---

### Filesystem Design Choices

#### General Filesystem Theory

Filesystems organize data on storage devices. Key concepts:

**1. Filesystem Components**

- **Superblock/Boot Sector**: Metadata about the filesystem
- **Allocation Table**: Tracks which blocks are used/free
- **Directory Structure**: Organizes files into hierarchy
- **Data Blocks**: Actual file content
- **Metadata**: File size, timestamps, permissions

**2. Allocation Strategies**

- **Contiguous**: Files stored in consecutive blocks (fast, but fragments)
- **Linked**: Each block points to next (no fragmentation, slow random access)
- **Indexed**: Index block points to all data blocks (fast, flexible)
- **Extent-based**: Contiguous runs with metadata (modern approach)

**3. Directory Structures**

- **Flat**: All files in one directory (simple, doesn't scale)
- **Hierarchical**: Tree of directories (universally used today)
- **B-tree based**: Efficient for large directories (NTFS, ext4)

#### Our Implementation: FAT12

We chose FAT12 (File Allocation Table, 12-bit) for its simplicity:

**FAT12 Structure:**

```text
[Boot Sector][FAT 1][FAT 2][Root Directory][Data Area]
     512B      9×512B  9×512B     14×512B      Rest
```

**Why Two FAT Copies?**

Redundancy! If FAT 1 gets corrupted, FAT 2 can recover the filesystem.

**Boot Sector Layout:**

```c
// Simplified BPB (BIOS Parameter Block)
struct BootSector {
    uint8_t  jmp[3];              // Jump instruction (EB 3C 90)
    char     oem[8];              // "MSWIN4.1"
    uint16_t bytes_per_sector;    // 512
    uint8_t  sectors_per_cluster; // 1 (for floppy)
    uint16_t reserved_sectors;    // 1 (boot sector itself)
    uint8_t  fat_count;           // 2 (redundancy)
    uint16_t root_entries;        // 224 (14 sectors)
    uint16_t total_sectors;       // 2880 (1.44MB)
    uint8_t  media_type;          // 0xF0 (floppy)
    uint16_t sectors_per_fat;     // 9
    // ... more fields
    uint16_t signature;           // 0xAA55 (bootable marker)
} __attribute__((packed));
```

**FAT12 Cluster Chain:**

Files are stored as linked lists of clusters:

```
File: "README.TXT" (5KB)
Clusters: 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8 -> 9 -> 10 -> 0xFFF (EOF)
```

**How FAT Entries Work:**

FAT12 uses 12 bits per entry, so entries are packed:

```
Byte 0:    [Entry 0 low 8 bits]
Byte 1:    [Entry 1 low 4][Entry 0 high 4]
Byte 2:    [Entry 1 high 8 bits]
Byte 3:    [Entry 2 low 8 bits]
...
```

**Special FAT12 Values:**

- `0x000`: Free cluster
- `0x001`: Reserved
- `0x002-0xFF6`: Valid cluster numbers
- `0xFF7`: Bad cluster
- `0xFF8-0xFFF`: End of file

**Our FAT12 Implementation:**

Stage 1 (assembly):
```assembly
; Calculate FAT index
mov ax, [current_cluster]
mov cx, 3
mul cx              ; AX = cluster * 3
mov cx, 2
div cx              ; AX = (cluster * 3) / 2, DX = remainder

; Load 16 bits from FAT
mov si, fat_buffer
add si, ax
mov ax, [si]

; Extract 12-bit value
test dx, dx         ; Check if even or odd
jz .even
    shr ax, 4       ; Odd cluster: shift right 4 bits
    jmp .next
.even:
    and ax, 0x0FFF  ; Even cluster: mask to 12 bits
.next:
```

Stage 2 (C):
```c
uint16_t FAT_NextCluster(uint16_t cluster) {
    uint32_t fat_index = (cluster * 3) / 2;
    uint16_t value;
    
    if (cluster % 2 == 0) {
        // Even cluster: low 12 bits
        value = *(uint16_t*)(&g_FAT[fat_index]) & 0x0FFF;
    } else {
        // Odd cluster: high 12 bits
        value = *(uint16_t*)(&g_FAT[fat_index]) >> 4;
    }
    
    return value;
}
```

**Directory Entry Structure:**

Each file has a 32-byte directory entry:

```c
struct DirectoryEntry {
    char     name[11];          // 8.3 format (KERNEL  BIN)
    uint8_t  attributes;        // Archive, hidden, system, etc.
    uint8_t  reserved;
    uint8_t  creation_time_ms;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t access_date;
    uint16_t cluster_high;      // High word (FAT32, 0 for FAT12)
    uint16_t modified_time;
    uint16_t modified_date;
    uint16_t cluster_low;       // First cluster
    uint32_t size;              // File size in bytes
} __attribute__((packed));
```

**Finding a File:**

```c
// Search root directory for filename
for (int i = 0; i < 224; i++) {
    if (memcmp(rootdir[i].name, "KERNEL  BIN", 11) == 0) {
        // Found! Start reading from cluster_low
        uint16_t cluster = rootdir[i].cluster_low;
        // Follow cluster chain...
    }
}
```

#### Alternative Filesystem Approaches

**1. FAT16/FAT32**

*How it works:* Similar to FAT12, but 16-bit or 32-bit cluster addresses
- **Pros**: Larger disks (FAT16: 2GB, FAT32: 2TB), widely compatible
- **Cons**: More complex, larger FAT tables, no journaling
- **Used by**: USB drives, SD cards, digital cameras
- **Our choice**: FAT12 simpler for learning, sufficient for 1.44MB floppy

**2. ext2/ext3/ext4 (Linux Filesystems)**

*How it works:* Inode-based with block groups
- **Pros**: 
  - Fast (optimized for hard drives)
  - Permissions and ownership
  - Journaling (ext3/ext4)
  - Large file support
- **Cons**: 
  - Complex to implement
  - Linux-centric
  - Requires permission system
- **Used by**: Most Linux systems
- **Our future**: Possible learning project after FAT12 is solid

**3. NTFS (Windows Filesystem)**

*How it works:* B-tree based Master File Table (MFT)
- **Pros**: 
  - Very robust (journaling, transactions)
  - Permissions and ACLs
  - Compression and encryption
  - Large file/volume support
- **Cons**: 
  - Extremely complex
  - Proprietary (documentation incomplete)
  - Requires significant infrastructure
- **Used by**: Windows NT, 2000, XP, Vista, 7, 8, 10, 11
- **Our choice**: Too complex for a learning OS

**4. Custom Simple Filesystem**

*How it works:* Design our own from scratch
- **Pros**: 
  - Learn filesystem design principles
  - Optimize for our specific needs
  - No compatibility baggage
- **Cons**: 
  - Can't mount on host OS without tools
  - No existing tools
  - Easy to make mistakes
- **Our future**: Interesting learning exercise after mastering FAT12

**Filesystem Comparison:**

| Filesystem | Complexity | Max Size | Features | Compatibility | Our Use |
|------------|------------|----------|----------|---------------|---------|
| FAT12 | Low | 32 MB | Basic | Universal | ✅ **Current** |
| FAT16 | Low | 2 GB | Basic | Universal | 🔄 Possible upgrade |
| FAT32 | Medium | 2 TB | Basic | Universal | 🔄 Long-term option |
| ext2 | Medium | 2 TB | Permissions | Linux | ⭐ Learning project |
| ext3/4 | High | 16 TB+ | Journaling | Linux | ❌ Too complex |
| NTFS | Very High | 256 TB | Advanced | Windows | ❌ Too complex |
| Custom | Variable | Any | Custom | None | ⭐ Future learning |

**Why FAT12 is Perfect for Learning:**

1. **Simple**: Can implement in ~200 lines of C or ~150 lines of assembly
2. **Well-Documented**: Microsoft's FAT specification is public
3. **Universal**: Works on Windows, Linux, macOS
4. **Debuggable**: Easy to inspect with hex editor
5. **Historical**: Understanding FAT helps understand modern filesystems

---

### CPU Modes and Privilege Levels

#### General CPU Mode Theory

x86 CPUs support multiple operating modes with different capabilities:

**1. Real Mode (16-bit)**

- Default mode at power-on
- 20-bit addressing (1 MB limit)
- Segmentation required
- No memory protection
- Direct hardware access
- BIOS interrupts available

**2. Protected Mode (32-bit)**

- Enabled via CR0 register
- 32-bit addressing (4 GB)
- Segmentation and/or paging
- Memory protection via rings
- Hardware virtualization support
- No BIOS (must write drivers)

**3. Long Mode (64-bit)**

- Extension of protected mode
- 48-bit virtual addresses (256 TB)
- Mandatory paging
- Simplified segmentation
- NX bit support
- FS/GS segments for thread-local storage

**4. Virtual 8086 Mode**

- Runs 16-bit code in protected mode
- Useful for DOS compatibility
- Can trap privileged instructions
- Used by Windows 9x, DOSBox

**5. System Management Mode (SMM)**

- Hidden mode for firmware
- Highest privilege level
- Used for power management, security
- Invisible to OS

#### Our Implementation

**Current: Real Mode → Protected Mode Transition**

Our bootloader starts in real mode and transitions the kernel to protected mode:

**Step 1: Prepare for Protected Mode (in Stage 2)**

```c
// 1. Disable interrupts (we'll set up new IDT later)
__asm__ volatile("cli");

// 2. Enable A20 line (access memory above 1MB)
void EnableA20(void) {
    // Method 1: Fast A20 (most reliable)
    uint8_t value = x86_inb(0x92);
    x86_outb(0x92, value | 2);
    
    // Method 2: Keyboard controller (slower, more compatible)
    // Wait for keyboard controller ready
    while (x86_inb(0x64) & 0x02);
    x86_outb(0x64, 0xD1);  // Write to output port
    while (x86_inb(0x64) & 0x02);
    x86_outb(0x60, 0xDF);  // Enable A20
}
```

**What is A20 Line?**

Historical baggage from original IBM PC:
- 8086 had 20 address lines (A0-A19) = 1 MB
- 8086 wrapped around after 1MB (address 0x100000 became 0x00000)
- Some DOS programs relied on this wraparound bug
- 80286+ had 24+ address lines but A20 was gated for compatibility
- Must explicitly enable A20 to access memory above 1MB

**Step 2: Set Up GDT**

```c
// Define GDT entries
struct GDTEntry gdt[3];

// Null descriptor (required by CPU)
gdt[0] = (struct GDTEntry){0, 0, 0, 0, 0, 0};

// Kernel code segment (selector 0x08)
gdt[1] = (struct GDTEntry){
    .limit_low = 0xFFFF,
    .base_low = 0x0000,
    .base_mid = 0x00,
    .access = 0x9A,        // Present, Ring 0, Code, Readable
    .granularity = 0xCF,   // 4KB pages, 32-bit
    .base_high = 0x00
};

// Kernel data segment (selector 0x10)
gdt[2] = (struct GDTEntry){
    .limit_low = 0xFFFF,
    .base_low = 0x0000,
    .base_mid = 0x00,
    .access = 0x92,        // Present, Ring 0, Data, Writable
    .granularity = 0xCF,   // 4KB pages, 32-bit
    .base_high = 0x00
};

// Load GDT
struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_descriptor = {
    .limit = sizeof(gdt) - 1,
    .base = (uint32_t)&gdt
};

__asm__ volatile("lgdt %0" : : "m"(gdt_descriptor));
```

**Step 3: Switch to Protected Mode**

```assembly
; Set PE bit in CR0
mov eax, cr0
or eax, 1
mov cr0, eax

; Far jump to flush CPU pipeline and load CS with code selector
jmp 0x08:protected_mode_entry

[bits 32]
protected_mode_entry:
    ; Now in 32-bit protected mode!
    ; Set up segment registers
    mov ax, 0x10      ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack
    mov esp, 0x90000
    mov ebp, esp
    
    ; Jump to kernel
    call 0x100000
```

**Why Far Jump?**

The far jump (`jmp 0x08:address`) serves two purposes:
1. Loads CS with new code selector (0x08)
2. Flushes CPU pipeline (instruction prefetch buffer)

**Current Privilege Level:**

Our kernel runs in Ring 0 (highest privilege):

```
Ring 0: Kernel (us)
Ring 1: Device drivers (unused)
Ring 2: Device drivers (unused)  
Ring 3: User programs (future)
```

**Segment Selectors Explained:**

```
Selector 0x08 = 0000 0000 0000 1000
                ^^^^ ^^^^ ^^^^ ^│││
                │              ││└─ RPL (Requested Privilege Level) = 0
                │              │└── TI (Table Indicator) = 0 (GDT)
                └──────────────┴─── Index = 1 (second GDT entry)

Selector 0x10 = 0000 0000 0001 0000 (index = 2, third GDT entry)
```

#### Alternative Approaches

**1. Stay in Real Mode**

*How it works:* Never transition to protected mode
- **Pros**: Simpler, BIOS available, easier debugging
- **Cons**: 1 MB limit, no protection, no modern features
- **Used by**: DOS, very simple embedded systems
- **Our choice**: Too limited, can't learn real OS concepts

**2. Switch to Long Mode (64-bit) Directly**

*How it works:* Real mode → Long mode, skip protected mode
- **Pros**: Modern, large address space, NX bit
- **Cons**: 
  - More complex setup (IA-32e page tables required before switch)
  - Must have 64-bit CPU
  - Skips learning protected mode concepts
- **Used by**: Modern 64-bit bootloaders (GRUB2 UEFI)
- **Our choice**: Too advanced for first OS, want to understand 32-bit first

**3. Use Virtual 8086 Mode**

*How it works:* Protected mode that can run real mode code
- **Pros**: Best of both worlds, can call BIOS in protected mode
- **Cons**: Complex to set up, limited usefulness
- **Used by**: Windows 9x DOS boxes, some embedded systems
- **Our choice**: Unnecessary complexity, moving away from real mode

**4. Unreal Mode / Big Real Mode**

*How it works:* Real mode with 4GB segments
- **Pros**: Access full 4GB in real mode, BIOS still works
- **Cons**: Undefined behavior, fragile, non-standard
- **Used by**: Some bootloaders, DOS extenders
- **Our choice**: Hacky, prefer clean protected mode transition

**CPU Mode Comparison:**

| Mode | Address Space | Protection | Paging | Complexity | Our Use |
|------|---------------|------------|--------|------------|---------|
| Real Mode | 1 MB | None | No | Low | ✅ Bootloader |
| Unreal Mode | 4 GB | None | No | Medium | ❌ Hacky |
| Protected Mode (no paging) | 4 GB | Rings | No | Medium | ✅ **Current** |
| Protected Mode (paging) | 4 GB virtual | Rings | Yes | High | 🔄 Future |
| Virtual 8086 | 1 MB | Rings | Yes | High | ❌ Unnecessary |
| Long Mode (64-bit) | 256 TB | Rings + NX | Yes | Very High | ⭐ Long-term goal |

**Our Rationale:**

1. Start in real mode (required by BIOS)
2. Move to protected mode without paging (simpler learning)
3. Add paging later (when needed for virtual memory)
4. Eventually move to 64-bit long mode (modern OS feature)

This progression teaches each concept in isolation before combining them.

---

### Interrupt Handling Strategies

#### General Interrupt Theory

Interrupts are how hardware and software signal events to the CPU:

**Types of Interrupts:**

**1. Hardware Interrupts (IRQs)**

External devices signal CPU:
- IRQ 0: Timer (PIT - Programmable Interval Timer)
- IRQ 1: Keyboard
- IRQ 2: Cascade (connects to second PIC)
- IRQ 3-7: Serial, parallel, floppy, etc.
- IRQ 8-15: RTC, mouse, IDE, etc.

**2. Software Interrupts**

Triggered by INT instruction:
- INT 0x10: BIOS video services
- INT 0x13: BIOS disk services
- INT 0x16: BIOS keyboard services
- INT 0x80: Linux system calls (historical)
- INT 0x21: DOS services (historical)

**3. CPU Exceptions**

CPU-generated errors:
- INT 0: Divide by zero
- INT 1: Debug exception
- INT 3: Breakpoint
- INT 6: Invalid opcode
- INT 13: General protection fault
- INT 14: Page fault

**Interrupt Processing:**

1. CPU receives interrupt signal
2. CPU pushes flags, CS, IP onto stack
3. CPU loads handler address from IVT/IDT
4. Handler executes
5. Handler calls IRET to return
6. CPU pops IP, CS, flags from stack

#### Our Implementation

**Real Mode: Interrupt Vector Table (IVT)**

In real mode, interrupt handlers are found via IVT at address 0x0000:

```
IVT Entry = 0x0000 + (interrupt_number * 4)
Each entry: [Offset:16][Segment:16]
```

Example: INT 0x10 handler at entry 0x10 * 4 = 0x0040

**Using BIOS Interrupts in Our Bootloader:**

```c
// x86.asm - BIOS interrupt wrapper
_x86_Video_WriteCharTeletype:
    push bp
    mov bp, sp
    
    mov ah, 0x0E          ; BIOS teletype function
    mov al, [bp + 4]      ; Get character parameter
    mov bh, [bp + 6]      ; Get page parameter
    
    int 0x10              ; Call BIOS
    
    pop bp
    ret
```

**Why We Use BIOS:**

In real mode, BIOS provides free device drivers:
- Video output
- Disk I/O
- Keyboard input
- Timer services
- Memory detection

**Protected Mode: Interrupt Descriptor Table (IDT)**

In protected mode, BIOS interrupts don't work. We must set up our own IDT:

```c
// IDT entry structure
struct IDTEntry {
    uint16_t offset_low;   // Handler address low 16 bits
    uint16_t selector;     // Code segment selector
    uint8_t  zero;         // Unused, set to 0
    uint8_t  type_attr;    // Type and attributes
    uint16_t offset_high;  // Handler address high 16 bits
} __attribute__((packed));

struct IDTEntry idt[256];  // 256 possible interrupts

// Set up an interrupt handler
void IDT_SetGate(int interrupt, void (*handler)(), uint16_t selector, uint8_t type) {
    uint32_t handler_addr = (uint32_t)handler;
    
    idt[interrupt].offset_low = handler_addr & 0xFFFF;
    idt[interrupt].offset_high = (handler_addr >> 16) & 0xFFFF;
    idt[interrupt].selector = selector;  // Code segment (0x08)
    idt[interrupt].zero = 0;
    idt[interrupt].type_attr = type;     // 0x8E = Present, Ring 0, 32-bit Interrupt Gate
}

// Load IDT
struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_descriptor = {
    .limit = sizeof(idt) - 1,
    .base = (uint32_t)&idt
};

__asm__ volatile("lidt %0" : : "m"(idt_descriptor));
```

**Example: Divide by Zero Handler**

```assembly
; Exception handler stub
divide_by_zero_handler:
    cli                    ; Disable interrupts
    push eax
    push ebx
    
    ; Print error message
    mov eax, 0xB8000
    mov byte [eax], 'E'
    mov byte [eax+1], 0x04  ; Red
    
    pop ebx
    pop eax
    
    ; Halt system
    cli
    hlt
```

```c
// Register the handler
IDT_SetGate(0, divide_by_zero_handler, 0x08, 0x8E);
```

**PIC (Programmable Interrupt Controller) Remapping:**

The original PC used two PICs (8259A chips) to handle hardware interrupts:

**Problem:** Default PIC IRQs conflict with CPU exceptions:
- IRQ 0-7 → INT 0x08-0x0F (conflicts with CPU exceptions 8-15)
- IRQ 8-15 → INT 0x70-0x77

**Solution:** Remap PICs to avoid conflict:

```c
void PIC_Remap(uint8_t offset1, uint8_t offset2) {
    // offset1: Master PIC vector offset (typically 0x20)
    // offset2: Slave PIC vector offset (typically 0x28)
    
    // Save masks
    uint8_t mask1 = x86_inb(0x21);
    uint8_t mask2 = x86_inb(0xA1);
    
    // Start initialization
    x86_outb(0x20, 0x11);  // Master PIC: ICW1 (init)
    x86_outb(0xA0, 0x11);  // Slave PIC: ICW1
    
    // Set vector offsets
    x86_outb(0x21, offset1);  // Master PIC: ICW2 (offset)
    x86_outb(0xA1, offset2);  // Slave PIC: ICW2
    
    // Tell PICs about each other
    x86_outb(0x21, 0x04);  // Master PIC: ICW3 (slave at IRQ2)
    x86_outb(0xA1, 0x02);  // Slave PIC: ICW3 (cascade identity)
    
    // Set mode
    x86_outb(0x21, 0x01);  // Master PIC: ICW4 (8086 mode)
    x86_outb(0xA1, 0x01);  // Slave PIC: ICW4
    
    // Restore masks
    x86_outb(0x21, mask1);
    x86_outb(0xA1, mask2);
}

// Usage:
PIC_Remap(0x20, 0x28);
// Now: IRQ 0-7 → INT 0x20-0x27
//      IRQ 8-15 → INT 0x28-0x2F
```

**After remapping:**
- IRQ 0 (Timer) → INT 0x20
- IRQ 1 (Keyboard) → INT 0x21
- IRQ 2 (Cascade) → INT 0x22
- ... and so on

**Enabling/Disabling Interrupts:**

```c
// Disable all interrupts
__asm__ volatile("cli");

// Enable interrupts
__asm__ volatile("sti");

// Mask specific IRQ (disable one device)
void IRQ_Mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? 0x21 : 0xA1;
    uint8_t value = x86_inb(port) | (1 << (irq % 8));
    x86_outb(port, value);
}

// Unmask IRQ (enable one device)
void IRQ_Unmask(uint8_t irq) {
    uint16_t port = (irq < 8) ? 0x21 : 0xA1;
    uint8_t value = x86_inb(port) & ~(1 << (irq % 8));
    x86_outb(port, value);
}
```

**Acknowledging Interrupts (EOI):**

After handling a hardware interrupt, must send End of Interrupt (EOI):

```c
void PIC_SendEOI(uint8_t irq) {
    if (irq >= 8) {
        x86_outb(0xA0, 0x20);  // Slave PIC EOI
    }
    x86_outb(0x20, 0x20);      // Master PIC EOI (always)
}
```

**Example: Timer Interrupt Handler**

```c
uint32_t tick_count = 0;

void timer_handler() {
    tick_count++;
    
    // Do timer-related work
    // ...
    
    // Send EOI
    PIC_SendEOI(0);
}

// Set up timer
IDT_SetGate(0x20, timer_handler, 0x08, 0x8E);
IRQ_Unmask(0);  // Enable IRQ 0
__asm__ volatile("sti");  // Enable interrupts globally
```

#### Alternative Approaches

**1. Polling Instead of Interrupts**

*How it works:* Continuously check device status
- **Pros**: Simpler, no interrupt handler complexity
- **Cons**: Wastes CPU, slow response, can't sleep
- **Used by**: Simple embedded systems, some early computers
- **Our choice**: Interrupts are essential for responsive OS

**2. APIC (Advanced PIC)**

*How it works:* Modern interrupt controller, replaces 8259 PIC
- **Pros**: 
  - More interrupts (224 vs 16)
  - Per-CPU interrupts (SMP)
  - Message-based (no EOI needed)
  - Better priority handling
- **Cons**: 
  - More complex to program
  - Requires APIC detection
  - Not available on all hardware
- **Used by**: Modern x86 systems, multi-core CPUs
- **Our future**: Will implement after basic PIC works

**3. MSI/MSI-X (Message Signaled Interrupts)**

*How it works:* PCIe devices write to memory-mapped address
- **Pros**: 
  - No shared IRQ lines
  - Better performance
  - More flexible
- **Cons**: 
  - Requires PCIe
  - Complex configuration
  - Need APIC
- **Used by**: Modern PCIe devices
- **Our future**: Long-term goal for PCIe support

**4. System Call Methods**

Different ways to invoke kernel from user space:

| Method | Speed | Complexity | Compatibility | Modern Use |
|--------|-------|------------|---------------|-----------|
| INT 0x80 | Slow | Low | Universal | ❌ Legacy (Linux < 2.6) |
| SYSENTER/SYSEXIT | Fast | Medium | Intel only | ✅ Linux x86 |
| SYSCALL/SYSRET | Fast | Medium | AMD64 | ✅ Linux x86_64 |
| Software Interrupt | Slow | Low | Universal | 🔄 Our current plan |

**Interrupt Comparison:**

| Approach | Latency | CPU Usage | Complexity | Our Use |
|----------|---------|-----------|------------|---------|
| Polling | High | Very High | Low | ❌ Inefficient |
| 8259 PIC | Medium | Low | Medium | ✅ **Current plan** |
| APIC | Low | Low | High | ⭐ Future (SMP) |
| MSI/MSI-X | Very Low | Very Low | Very High | ⭐ Long-term |

**Our Rationale:**

1. Use BIOS interrupts in real mode (free drivers)
2. Set up IDT and PIC in protected mode (required)
3. Later add APIC for multi-core support
4. Eventually support MSI for modern devices

Each step builds on the previous, teaching interrupt concepts incrementally.

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
0x00100000    - Kernel loaded here (Protected Mode)
```

### Key Advantages of Two-Stage Design

- **More Space**: Stage 2 can be any size, not limited to 512 bytes
- **High-Level Language**: Stage 2 can be written in C for better maintainability
- **Error Handling**: Room for detailed error messages and diagnostics
- **Modularity**: Can load multiple components (drivers, config files, etc.)
- **Flexibility**: Easy to add features without fighting size constraints

---

## Visual Architecture and Data Flow

This section provides visual representations of how data flows through the system and how components interact.

### Complete System Boot Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         POWER ON                                │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                    BIOS POST & Init                             │
│  • Hardware detection                                           │
│  • Memory test                                                  │
│  • Identify boot devices                                        │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│              Load Boot Sector (512 bytes)                       │
│  • Read LBA 0 (sector 0) from boot disk                        │
│  • Load to memory at 0x7C00                                     │
│  • Verify boot signature (0xAA55)                               │
│  • Jump to 0x7C00                                               │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                  Stage 1 Bootloader                             │
│                   (boot.asm - 512 bytes)                        │
│  ┌───────────────────────────────────────────────────┐          │
│  │ 1. Initialize segments (DS, ES, SS, CS = 0)      │          │
│  │ 2. Set stack at 0x7C00 (grows down)              │          │
│  │ 3. Detect disk geometry (INT 13h, AH=08h)        │          │
│  └───────────────────────┬───────────────────────────┘          │
│                          ▼                                      │
│  ┌───────────────────────────────────────────────────┐          │
│  │ 4. Read FAT12 structures:                         │          │
│  │    • Root directory (sector 19, 14 sectors)       │          │
│  │    • FAT table (sector 1, 9 sectors)              │          │
│  └───────────────────────┬───────────────────────────┘          │
│                          ▼                                      │
│  ┌───────────────────────────────────────────────────┐          │
│  │ 5. Search for "STAGE2  BIN" in root dir           │          │
│  │    • Compare 11-character filename                │          │
│  │    • Get first cluster number                     │          │
│  └───────────────────────┬───────────────────────────┘          │
│                          ▼                                      │
│  ┌───────────────────────────────────────────────────┐          │
│  │ 6. Load Stage 2 into memory:                      │          │
│  │    • Start at 0x0500                              │          │
│  │    • Follow FAT12 cluster chain                   │          │
│  │    • Load until EOF (cluster >= 0xFF8)            │          │
│  └───────────────────────┬───────────────────────────┘          │
│                          ▼                                      │
│  ┌───────────────────────────────────────────────────┐          │
│  │ 7. Jump to Stage 2 entry point (0x0500)           │          │
│  └───────────────────────────────────────────────────┘          │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                  Stage 2 Bootloader                             │
│              (main.asm + main.c + helpers)                      │
│  ┌───────────────────────────────────────────────────┐          │
│  │ 1. Assembly Entry (main.asm):                     │          │
│  │    • Initialize segments                          │          │
│  │    • Set up stack (SP=0, SS=DS)                   │          │
│  │    • Pass boot drive to C code (DX register)      │          │
│  │    • Call C entry point (_cstart_)                │          │
│  └───────────────────────┬───────────────────────────┘          │
│                          ▼                                      │
│  ┌───────────────────────────────────────────────────┐          │
│  │ 2. C Entry Point (main.c):                        │          │
│  │    • Display "Stage 2 loading..."                 │          │
│  │    • Initialize disk subsystem                    │          │
│  │    • Initialize FAT filesystem                    │          │
│  └───────────────────────┬───────────────────────────┘          │
│                          ▼                                      │
│  ┌───────────────────────────────────────────────────┐          │
│  │ 3. Load Kernel:                                   │          │
│  │    • Search for "KERNEL  BIN"                     │          │
│  │    • Load to buffer                               │          │
│  │    • Copy to 0x100000 (1MB mark)                  │          │
│  └───────────────────────┬───────────────────────────┘          │
│                          ▼                                      │
│  ┌───────────────────────────────────────────────────┐          │
│  │ 4. Prepare for Protected Mode:                    │          │
│  │    • Enable A20 line                              │          │
│  │    • Set up GDT (Global Descriptor Table)         │          │
│  │    • Load GDT register                            │          │
│  └───────────────────────┬───────────────────────────┘          │
│                          ▼                                      │
│  ┌───────────────────────────────────────────────────┐          │
│  │ 5. Switch to Protected Mode:                      │          │
│  │    • Set PE bit in CR0                            │          │
│  │    • Far jump to flush pipeline                   │          │
│  │    • Set up 32-bit segments                       │          │
│  │    • Jump to kernel at 0x100000                   │          │
│  └───────────────────────────────────────────────────┘          │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                    32-bit Kernel                                │
│                  (entry.asm + main.c)                           │
│  ┌───────────────────────────────────────────────────┐          │
│  │ 1. Entry Point (entry.asm):                       │          │
│  │    • Set up stack at 0x200000 (2MB)               │          │
│  │    • Call C kernel entry (start)                  │          │
│  └───────────────────────┬───────────────────────────┘          │
│                          ▼                                      │
│  ┌───────────────────────────────────────────────────┐          │
│  │ 2. Kernel Main (main.c):                          │          │
│  │    • Clear VGA text buffer (0xB8000)              │          │
│  │    • Write "Hello world from kernel"              │          │
│  │    • Infinite loop (halt)                         │          │
│  └───────────────────────────────────────────────────┘          │
│                                                                 │
│  Future: IDT, Interrupts, Memory Manager, Drivers, Scheduler   │
└─────────────────────────────────────────────────────────────────┘
                         │
                         ▼
                   OS RUNNING
```

### Memory Layout During Boot Process

```
Real Mode (16-bit) Memory Map - Bootloader Stage
═════════════════════════════════════════════════════════════

0x00000000  ┌─────────────────────────────────────┐
            │  Interrupt Vector Table (IVT)        │  1 KB
            │  256 entries × 4 bytes               │
0x00000400  ├─────────────────────────────────────┤
            │  BIOS Data Area (BDA)                │  256 B
0x00000500  ├─────────────────────────────────────┤
            │                                      │
            │  Stage 2 Bootloader                  │
            │  Loaded here by Stage 1              │  ~30 KB
            │  (C code + assembly + data)          │
            │                                      │
0x00007C00  ├─────────────────────────────────────┤
            │  Stage 1 Bootloader                  │  512 B
            │  Loaded here by BIOS                 │
0x00007E00  ├─────────────────────────────────────┤
            │                                      │
            │  Disk Read Buffer                    │
            │  FAT, Root Dir, File data            │  ~600 KB
            │                                      │
0x0009FC00  ├─────────────────────────────────────┤
            │  Extended BIOS Data Area (EBDA)      │  ~1 KB
0x000A0000  ├─────────────────────────────────────┤
            │  Video Memory (VGA)                  │
            │  0xB8000 = Text mode buffer          │  128 KB
0x000C0000  ├─────────────────────────────────────┤
            │  ROM BIOS and Adapter ROMs           │  256 KB
0x00100000  └─────────────────────────────────────┘

Protected Mode (32-bit) Memory Map - Kernel Stage
═════════════════════════════════════════════════════════════

0x00000000  ┌─────────────────────────────────────┐
            │  Real Mode Legacy Area               │  1 MB
            │  (Still contains Stage 1 & 2)        │
0x00100000  ├─────────────────────────────────────┤ ← 1 MB
            │                                      │
            │  Kernel Code (.text)                 │
            │  Kernel Data (.data, .bss)           │  ~1 MB
            │                                      │
0x00200000  ├─────────────────────────────────────┤ ← 2 MB
            │                                      │
            │  Kernel Stack                        │
            │  (Grows downward from 2MB)           │  ~1 MB
            │                                      │
0x00300000  ├─────────────────────────────────────┤ ← 3 MB
            │                                      │
            │  Free Memory                         │
            │  (Future: Heap, Page Tables,         │
            │   User Programs, etc.)               │
            │                                      │
            │                                      │
            ▼                                      ▼

Note: Actual RAM ends at physical limit (e.g., 16MB, 32MB, etc.)
```

### FAT12 Filesystem Structure

```
Floppy Disk Image (1.44 MB = 2880 sectors × 512 bytes)
═══════════════════════════════════════════════════════════════

Sector 0      ┌───────────────────────────────────────┐
(LBA 0)       │  Boot Sector                          │
              │  • Jump instruction (3 bytes)          │
              │  • BPB (BIOS Parameter Block)          │
              │  • Boot code (Stage 1)                 │
              │  • Boot signature (0xAA55)             │
Sector 1      ├───────────────────────────────────────┤
              │                                        │
              │  FAT #1 (File Allocation Table)       │
              │  9 sectors × 512 bytes = 4608 bytes    │
              │  2880 clusters × 12 bits = 4320 bytes  │
              │  (plus some padding)                   │
              │                                        │
Sector 10     ├───────────────────────────────────────┤
              │                                        │
              │  FAT #2 (Backup copy)                  │
              │  Exact duplicate of FAT #1             │
              │  9 sectors                             │
              │                                        │
Sector 19     ├───────────────────────────────────────┤
              │                                        │
              │  Root Directory                        │
              │  224 entries × 32 bytes = 7168 bytes   │
              │  14 sectors                            │
              │                                        │
              │  Entry format:                         │
              │  • Name (11 bytes, 8.3 format)         │
              │  • Attributes (1 byte)                 │
              │  • Reserved (10 bytes)                 │
              │  • Time/Date (4 bytes)                 │
              │  • First cluster (2 bytes)             │
              │  • File size (4 bytes)                 │
              │                                        │
Sector 33     ├───────────────────────────────────────┤
              │                                        │
              │                                        │
              │  Data Area                             │
              │  (Clusters 2-2847)                     │
              │                                        │
              │  Files stored here:                    │
              │  • STAGE2.BIN                          │
              │  • KERNEL.BIN                          │
              │  • TEST.TXT                            │
              │                                        │
              │  Each cluster = 1 sector = 512 bytes   │
              │                                        │
              │                                        │
Sector 2880   └───────────────────────────────────────┘

FAT12 Cluster Chain Example:
════════════════════════════════════════════════════════════

File: KERNEL.BIN (6 KB = 12 sectors = 12 clusters)

Directory Entry → First Cluster: 2

FAT Table:
  Cluster 2  →  3     (next cluster is 3)
  Cluster 3  →  4     (next cluster is 4)
  Cluster 4  →  5     (next cluster is 5)
  Cluster 5  →  6     (next cluster is 6)
  Cluster 6  →  7     (next cluster is 7)
  Cluster 7  →  8     (next cluster is 8)
  Cluster 8  →  9     (next cluster is 9)
  Cluster 9  →  10    (next cluster is 10)
  Cluster 10 →  11    (next cluster is 11)
  Cluster 11 →  12    (next cluster is 12)
  Cluster 12 →  13    (next cluster is 13)
  Cluster 13 →  0xFFF (End of file)

Reading sequence:
  1. Read cluster 2 → Sector 33 (LBA = 33 + (2-2) = 33)
  2. Read cluster 3 → Sector 34 (LBA = 33 + (3-2) = 34)
  3. ... continue ...
  12. Read cluster 13 → Sector 44 (LBA = 33 + (13-2) = 44)
  13. See 0xFFF → File complete!
```

### Protected Mode Transition

```
CPU Mode Transition: Real Mode → Protected Mode
════════════════════════════════════════════════════════════

Real Mode (16-bit)                Protected Mode (32-bit)
┌───────────────────┐            ┌───────────────────┐
│ Segmented Memory  │            │ Flat Memory       │
│ segment:offset    │            │ Linear addresses  │
│ 20-bit addresses  │   SWITCH   │ 32-bit addresses  │
│ 1 MB limit        │  ───────>  │ 4 GB limit        │
│ No protection     │            │ Memory protection │
│ BIOS available    │            │ No BIOS           │
└───────────────────┘            └───────────────────┘

Transition Steps:
════════════════════════════════════════════════════════════

1. Disable Interrupts
   ┌──────────────────────────────────┐
   │ CLI                              │  Clear interrupt flag
   └──────────────────────────────────┘

2. Enable A20 Line
   ┌──────────────────────────────────┐
   │ IN   AL, 0x92                    │  Read from port 0x92
   │ OR   AL, 2                       │  Set bit 1
   │ OUT  0x92, AL                    │  Write back
   └──────────────────────────────────┘
   
   Why? A20 line gates address bit 20
   • Disabled: Memory wraps at 1MB (8086 compatibility)
   • Enabled: Can access memory above 1MB

3. Set Up GDT (Global Descriptor Table)
   ┌──────────────────────────────────────────────────────┐
   │ GDT Entry 0: Null Descriptor (required)              │
   │   Base: 0x00000000, Limit: 0x00000, Access: 0x00     │
   ├──────────────────────────────────────────────────────┤
   │ GDT Entry 1: Code Segment (Selector 0x08)            │
   │   Base: 0x00000000, Limit: 0xFFFFF, Access: 0x9A     │
   │   Flags: 4KB granularity, 32-bit, Present, Ring 0    │
   │   Type: Code, Executable, Readable                   │
   ├──────────────────────────────────────────────────────┤
   │ GDT Entry 2: Data Segment (Selector 0x10)            │
   │   Base: 0x00000000, Limit: 0xFFFFF, Access: 0x92     │
   │   Flags: 4KB granularity, 32-bit, Present, Ring 0    │
   │   Type: Data, Writable                               │
   └──────────────────────────────────────────────────────┘

4. Load GDT Register
   ┌──────────────────────────────────┐
   │ LGDT [gdt_descriptor]            │  Load GDT
   │                                  │
   │ gdt_descriptor:                  │
   │   .limit = sizeof(gdt) - 1       │
   │   .base  = &gdt                  │
   └──────────────────────────────────┘

5. Set Protected Mode Bit
   ┌──────────────────────────────────┐
   │ MOV  EAX, CR0                    │  Read CR0 control register
   │ OR   EAX, 1                      │  Set PE (Protection Enable) bit
   │ MOV  CR0, EAX                    │  Write back to CR0
   └──────────────────────────────────┘

6. Far Jump (Flush Pipeline)
   ┌──────────────────────────────────┐
   │ JMP  0x08:protected_mode_entry   │  Jump to code selector
   └──────────────────────────────────┘
   
   Why far jump?
   • Loads CS with new code selector (0x08)
   • Flushes CPU instruction prefetch buffer
   • Ensures execution continues in protected mode

7. Set Up Segment Registers [Now in 32-bit mode]
   ┌──────────────────────────────────┐
   │ [BITS 32]                        │
   │ protected_mode_entry:            │
   │   MOV  AX, 0x10                  │  Data segment selector
   │   MOV  DS, AX                    │  Set DS
   │   MOV  ES, AX                    │  Set ES
   │   MOV  FS, AX                    │  Set FS
   │   MOV  GS, AX                    │  Set GS
   │   MOV  SS, AX                    │  Set SS
   │   MOV  ESP, 0x90000              │  Set 32-bit stack pointer
   └──────────────────────────────────┘

8. Jump to Kernel
   ┌──────────────────────────────────┐
   │ CALL 0x100000                    │  Call kernel entry point
   └──────────────────────────────────┘

Result: CPU now in protected mode!
• 32-bit instructions
• 4 GB address space
• Memory protection via segments
• Can set up paging for virtual memory
• No BIOS (must write own drivers)
```

### Build System Data Flow

```
Source Files → Build Process → Bootable Disk Image
════════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────┐
│                     SOURCE FILES                            │
├─────────────────────────────────────────────────────────────┤
│  Stage 1: boot.asm (Assembly)                               │
│  Stage 2: main.asm, main.c, stdio.c, x86.asm, ...          │
│  Kernel:  entry.asm, main.c                                 │
│  Tools:   fat.c (FAT12 reader utility)                      │
└────────────┬────────────────────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────┐
│                  COMPILATION PHASE                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Stage 1:                                                   │
│  ┌────────────────────────────────────┐                    │
│  │ boot.asm  →  NASM -f bin  →  stage1.bin (512 B)        │
│  └────────────────────────────────────┘                    │
│                                                             │
│  Stage 2:                                                   │
│  ┌────────────────────────────────────┐                    │
│  │ main.asm  →  NASM -f obj  →  main.obj                  │
│  │ x86.asm   →  NASM -f obj  →  x86.obj                   │
│  │ main.c    →  WCC (16-bit) →  main_c.obj                │
│  │ stdio.c   →  WCC (16-bit) →  stdio.obj                 │
│  │ ...                                                      │
│  └────────────────────┬───────────────┘                    │
│                       ▼                                     │
│  ┌────────────────────────────────────┐                    │
│  │ WLINK (linker)                     │                    │
│  │ • Combine all .obj files           │                    │
│  │ • Apply linker script (linker.lnk) │                    │
│  │ • Output raw binary                │                    │
│  │  →  stage2.bin (~2-10 KB)          │                    │
│  └────────────────────────────────────┘                    │
│                                                             │
│  Kernel:                                                    │
│  ┌────────────────────────────────────┐                    │
│  │ entry.asm →  NASM -f elf32  →  entry.o                 │
│  │ main.c    →  GCC -m32       →  main.o                  │
│  └────────────────────┬───────────────┘                    │
│                       ▼                                     │
│  ┌────────────────────────────────────┐                    │
│  │ LD (GNU linker)                    │                    │
│  │ • Combine .o files                 │                    │
│  │ • Apply linker script (linker.ld)  │                    │
│  │ • Output raw binary at 0x100000    │                    │
│  │  →  kernel.bin (~1-5 KB)           │                    │
│  └────────────────────────────────────┘                    │
│                                                             │
│  Tools (host):                                              │
│  ┌────────────────────────────────────┐                    │
│  │ fat.c  →  GCC (native)  →  fat (executable)            │
│  └────────────────────────────────────┘                    │
└────────────┬────────────────────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────┐
│              DISK IMAGE CREATION PHASE                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. Create blank image:                                     │
│  ┌────────────────────────────────────┐                    │
│  │ dd if=/dev/zero of=floppy.img      │                    │
│  │    bs=512 count=2880                │                    │
│  │  →  Blank 1.44 MB image            │                    │
│  └────────────────────────────────────┘                    │
│                                                             │
│  2. Format as FAT12:                                        │
│  ┌────────────────────────────────────┐                    │
│  │ mkfs.fat -F 12 -n "NBOS" floppy.img│                    │
│  │  →  FAT12 filesystem structures    │                    │
│  │     (Boot sector, FATs, Root dir)   │                    │
│  └────────────────────────────────────┘                    │
│                                                             │
│  3. Install bootloader:                                     │
│  ┌────────────────────────────────────┐                    │
│  │ dd if=stage1.bin of=floppy.img     │                    │
│  │    conv=notrunc                     │                    │
│  │  →  Bootloader in sector 0         │                    │
│  │     (Preserves FAT filesystem)      │                    │
│  └────────────────────────────────────┘                    │
│                                                             │
│  4. Copy files to filesystem:                               │
│  ┌────────────────────────────────────┐                    │
│  │ mcopy -i floppy.img stage2.bin ::/ │                    │
│  │ mcopy -i floppy.img kernel.bin ::/ │                    │
│  │ mcopy -i floppy.img test.txt ::/   │                    │
│  │  →  Files in FAT12 root directory  │                    │
│  └────────────────────────────────────┘                    │
└────────────┬────────────────────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────┐
│              FINAL DISK IMAGE                               │
│              main_floppy.img (1.44 MB)                      │
├─────────────────────────────────────────────────────────────┤
│  • Bootable (0xAA55 signature)                              │
│  • FAT12 filesystem                                         │
│  • Stage 1 in boot sector                                   │
│  • Stage 2, Kernel, Test file in root directory             │
│  • Can be used with QEMU, VirtualBox, VMware, real hardware │
└─────────────────────────────────────────────────────────────┘
```

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
#include "disk.h"
#include "fat.h"
#include "memory.h"

void _cdecl cstart_(uint16_t bootDrive)
{
    // Initialize Disk and FAT
    // Load Kernel from /kernel.bin
    // Copy Kernel to 0x100000 (1MB)
    // Jump to Kernel
}
```

**Key responsibilities:**
- Initialize disk driver with boot drive number
- Initialize FAT12 filesystem driver
- Load `kernel.bin` from the filesystem
- Copy kernel to protected mode memory (1MB mark)
- Transfer control to the kernel

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

### Kernel (entry.asm + main.c)

The kernel is now a 32-bit protected mode program written in C.

#### entry.asm - Kernel Entry Point

```assembly
bits 32
global _start
extern start

section .entry
_start:
    ; Set up stack for kernel at 2MB mark
    mov esp, 0x200000
    mov ebp, esp

    call start
    cli
    hlt
```

**Responsibilities:**
- Establish a valid stack environment (stack pointer at `0x200000`)
- Call the C kernel entry point (`start`)
- Halt the CPU if the kernel returns

#### main.c - C Kernel Implementation

```c
void _cdecl start(uint16_t bootDrive)
{
    // Write "Hello world from kernel" to video memory
    const char *str = "Hello world from kernel";
    uint8_t *video = (uint8_t *)0xB8000;
    
    // ... (write string to video memory) ...
    
    for (;;);
}
```

The kernel currently writes directly to VGA text mode memory (`0xB8000`) to display a message, proving that we are successfully executing C code in protected mode.

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

4. **Kernel Execution**
   - **Loading**:
     - Stage 2 opens `kernel.bin` using FAT12 driver
     - Reads file to a temporary low-memory buffer (e.g., `0x30000`)
     - Copies kernel to `0x100000` (1MB mark) in high memory
   - **Execution**:
     - Stage 2 jumps to kernel entry point
     - `entry.asm` sets up the stack at `0x200000`
     - Calls C function `start()`
     - Kernel writes "Hello world from kernel" directly to video memory (`0xB8000`)
     - System halts

### Memory Map During Execution

```text
Address Range                      Contents                                  Size      Access
================================================================================================
0x0000:0x0000 - 0x0000:0x03FF     Interrupt Vector Table (IVT)              1 KB      Read/Write
0x0000:0x0400 - 0x0000:0x04FF     BIOS Data Area (BDA)                      256 bytes Read/Write
0x0000:0x0500 - 0x0000:0x7BFF     Stage 2 Bootloader (loaded here)          ~29.5 KB  Read/Execute
0x0000:0x7C00 - 0x0000:0x7DFF     Stage 1 Bootloader (BIOS loads here)      512 bytes Read/Execute
0x0000:0x7E00 - 0x0000:0xFFFF     FAT buffer, stack, free memory            ~32.5 KB  Read/Write
0x00010000    - 0x0009FFFF        Free conventional memory                  ~576 KB   Available
0x000A0000    - 0x000BFFFF        Video memory (text mode @ B8000)          128 KB    Read/Write
0x000C0000    - 0x000FFFFF        BIOS ROM, hardware mappings               256 KB    Read-only
0x00100000    - ...               Kernel (Protected Mode)                   -         Execute
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
- ❌ Keyboard input handling
- ❌ Memory management (paging, heap)
- ❌ Interrupt Descriptor Table (IDT) setup
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

## Toolchain and Build System Design

This section explains the choices made for development tools and build system architecture.

### Compiler and Assembler Choices

#### NASM (Netwide Assembler)

**Why NASM over alternatives?**

**NASM vs Other Assemblers:**

| Assembler | Syntax | OS Dev Support | Documentation | Our Choice |
|-----------|--------|----------------|---------------|------------|
| NASM | Intel | ✅ Excellent | Extensive | ✅ **Chosen** |
| GAS (GNU AS) | AT&T | ✅ Good | Good | ❌ Confusing syntax |
| FASM | Intel | ✅ Excellent | Good | ❌ Less common |
| YASM | Intel | ✅ Good | Good | ❌ Less active |
| MASM | Intel | ⚠️ Windows-only | Good | ❌ Not cross-platform |

**NASM Advantages for Our Project:**

1. **Intel Syntax**: More intuitive (`mov dest, src` not `mov src, dest`)
2. **Multiple Output Formats**: Raw binary, ELF, COFF, etc.
3. **Excellent Documentation**: Clear manual with examples
4. **Cross-Platform**: Runs on Linux, Windows, macOS
5. **Active Development**: Regular updates and bug fixes
6. **OS Dev Community**: Widely used in OS tutorials
7. **Macro System**: Powerful preprocessor for code reuse

**Example NASM Features We Use:**

```assembly
; Conditional compilation
%ifdef DEBUG
    mov si, debug_msg
    call puts
%endif

; Macros for code reuse
%macro PRINT_STRING 1
    mov si, %1
    call puts
%endmacro

; Include files
%include "constants.inc"

; Multiple output formats
; nasm -f bin boot.asm -o boot.bin    (raw binary)
; nasm -f elf32 kernel.asm -o kernel.o (Linux object)
; nasm -f obj stage2.asm -o stage2.obj (Watcom object)
```

**Why Not GAS?**

GAS (GNU Assembler) uses AT&T syntax which is less intuitive for beginners:

```assembly
# GAS (AT&T syntax)
movl $1, %eax          # Move immediate 1 to EAX
movl %eax, %ebx        # Move EAX to EBX

# NASM (Intel syntax)
mov eax, 1             ; Move immediate 1 to EAX
mov ebx, eax           ; Move EAX to EBX
```

AT&T syntax also has:
- Size suffixes: `movb`, `movw`, `movl`, `movq`
- Different immediate prefix: `$` for immediate, `%` for registers
- Reversed operand order: `instruction source, dest`

**Why Not FASM?**

FASM (Flat Assembler) is excellent but less commonly used in tutorials. NASM has more online resources for learning.

#### Open Watcom C Compiler

**Why Open Watcom for 16-bit Code?**

**Compiler Comparison for 16-bit x86:**

| Compiler | 16-bit Support | Modern | Open Source | Cross-Platform | Our Use |
|----------|----------------|--------|-------------|----------------|---------|
| Open Watcom | ✅ Native | ✅ Active | ✅ Yes | ✅ Yes | ✅ **Stage 2** |
| GCC | ⚠️ Limited (ia16-elf-gcc) | ✅ Active | ✅ Yes | ✅ Yes | ❌ Complex |
| Turbo C | ✅ Native | ❌ 1990s | ⚠️ Freeware | ❌ DOS only | ❌ Obsolete |
| Borland C | ✅ Native | ❌ 2000s | ❌ No | ❌ Windows only | ❌ Obsolete |
| Clang | ❌ No | ✅ Active | ✅ Yes | ✅ Yes | ❌ No 16-bit |

**Open Watcom Advantages:**

1. **Native 16-bit Support**: Designed for real mode programming
2. **Multiple Memory Models**: Tiny, small, medium, compact, large, huge
3. **Bare Metal Ready**: Can compile without C runtime (`-zl` flag)
4. **Excellent Optimizer**: Produces compact code (important for bootloaders)
5. **Good Documentation**: Clear manual with examples
6. **Cross-Platform**: Runs on Linux, Windows, macOS
7. **Active Maintenance**: Community-maintained fork is active
8. **Multiple Targets**: 16-bit, 32-bit, DOS, OS/2, Windows, QNX

**Memory Models Explained:**

Open Watcom supports 6 memory models for 16-bit code:

| Model | Code | Data | Use Case | Our Use |
|-------|------|------|----------|---------|
| Tiny | < 64KB | < 64KB | .COM programs | ❌ Too small |
| Small | < 64KB | < 64KB | Small programs | ✅ **Stage 2** |
| Medium | > 64KB | < 64KB | Large code | ❌ Unnecessary |
| Compact | < 64KB | > 64KB | Large data | ❌ Unnecessary |
| Large | > 64KB | > 64KB | Both large | ❌ Complex |
| Huge | > 64KB | > 64KB + huge arrays | Massive programs | ❌ Overkill |

**We Use Small Model (`-ms`):**

```bash
wcc -4 -d3 -s -wx -ms -zl -zq -fo=output.obj source.c
```

**Flags Explained:**

- `-4`: Generate 80386 instructions (backwards compatible to 8086)
- `-d3`: Full debugging info (line numbers, local variables, etc.)
- `-s`: Remove stack overflow checks (saves ~20 bytes per function)
- `-wx`: Maximum warning level (catch potential bugs)
- `-ms`: **Small memory model** (code and data each fit in 64KB)
- `-zl`: No default libraries (we provide our own functions)
- `-zq`: Quiet mode (suppress compiler banner)

**Why These Flags?**

- **No Standard Library (`-zl`)**: We're bare metal, no printf/malloc/etc available
- **Stack Check Removal (`-s`)**: Space-critical, manual stack management
- **Max Warnings (`-wx`)**: Catch bugs early
- **Debugging (`-d3`)**: Essential for troubleshooting low-level code

**Alternative: ia16-elf-gcc**

There's a GCC fork that targets 16-bit x86:

```bash
# ia16-elf-gcc (GCC fork for 16-bit)
ia16-elf-gcc -march=i8086 -mregparmcall -ffreestanding -nostdlib
```

**Why Not ia16-elf-gcc?**

- **Less mature**: Newer project, fewer users
- **Installation complex**: Requires building from source
- **Less documentation**: Limited OS dev examples
- **Different calling conventions**: More work to interface with assembly

However, it's a viable alternative for future projects.

#### GCC for 32-bit Kernel

**Why GCC for Protected Mode Kernel?**

Once we transition to 32-bit protected mode, we switch to GCC:

```bash
gcc -m32 -ffreestanding -nostdlib -c kernel.c -o kernel.o
```

**GCC Advantages for 32-bit:**

1. **Industry Standard**: Most OS dev tutorials use GCC
2. **Excellent Optimization**: `-O2`, `-Os` produce efficient code
3. **Inline Assembly**: Easy to mix C and assembly
4. **Cross-Compilation**: Can target any architecture
5. **Debugging Support**: Works great with GDB
6. **Extensive Documentation**: Massive community knowledge base

**GCC Flags for Kernel:**

```bash
gcc -m32                    # Target 32-bit (even on 64-bit host)
    -ffreestanding          # Freestanding environment (no hosted)
    -nostdlib               # Don't link with standard library
    -nostartfiles           # Don't use standard startup files
    -nodefaultlibs          # Don't use default libraries
    -fno-builtin            # Don't use builtin functions
    -fno-stack-protector    # No stack canary (we control stack)
    -fno-pie                # No Position Independent Executable
    -O2                     # Optimize for speed
    -Wall -Wextra           # All warnings
    -c                      # Compile only (don't link)
    kernel.c -o kernel.o
```

**Why These Flags?**

- **Freestanding (`-ffreestanding`)**: Tell GCC we're bare metal
- **No Standard Library (`-nostdlib`)**: We provide our own
- **No Builtin Functions (`-fno-builtin`)**: Don't assume strlen/memcpy/etc exist
- **No Stack Protector (`-fno-stack-protector`)**: Requires runtime support we don't have
- **32-bit (`-m32`)**: Even on 64-bit host, compile for 32-bit

**Linker (ld) for Kernel:**

```bash
ld -m elf_i386                      # 32-bit ELF
   -T linker.ld                     # Custom linker script
   -o kernel.bin                    # Output file
   entry.o kernel.o                 # Input objects
```

**Custom Linker Script (`linker.ld`):**

```ld
ENTRY(_start)
OUTPUT_FORMAT(binary)

SECTIONS
{
    . = 0x100000;                   /* Kernel at 1MB mark */
    
    .entry : ALIGN(16) {
        *(.entry)                   /* Entry point first */
    }
    
    .text : ALIGN(16) {
        *(.text)                    /* Code */
    }
    
    .rodata : ALIGN(16) {
        *(.rodata)                  /* Read-only data */
    }
    
    .data : ALIGN(16) {
        *(.data)                    /* Initialized data */
    }
    
    .bss : ALIGN(16) {
        *(COMMON)
        *(.bss)                     /* Uninitialized data */
    }
}
```

**Why Custom Linker Script?**

Default linker script creates ELF executable with headers. We need:
1. Raw binary format (no ELF headers)
2. Specific load address (1MB)
3. Entry point first (`.entry` section)
4. Aligned sections (performance)

### Build System Architecture

#### Modular Makefile Design

**Why Modular Makefiles?**

Instead of one giant Makefile, we use a hierarchical system:

```
Makefile (root)
├── src/bootloader/stage1/Makefile
├── src/bootloader/stage2/Makefile
└── src/kernel/Makefile
```

**Benefits:**

1. **Separation of Concerns**: Each component builds independently
2. **Parallel Builds**: `make -j4` can build stages simultaneously
3. **Easier Maintenance**: Changes to kernel don't affect bootloader builds
4. **Reusability**: Can build just one component: `cd src/kernel && make`
5. **Clarity**: Each Makefile is small and focused

**Root Makefile:**

```makefile
# High-level orchestration
.PHONY: all clean stage1 stage2 kernel floppy_image

all: floppy_image

floppy_image: stage1 stage2 kernel
	dd if=/dev/zero of=$(BUILD_DIR)/main_floppy.img bs=512 count=2880
	mkfs.fat -F 12 -n "NBOS" $(BUILD_DIR)/main_floppy.img
	dd if=$(BUILD_DIR)/stage1.bin of=$(BUILD_DIR)/main_floppy.img conv=notrunc
	mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/stage2.bin "::/stage2.bin"
	mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/kernel.bin "::/kernel.bin"

stage1:
	$(MAKE) -C src/bootloader/stage1 BUILD_DIR=$(abspath $(BUILD_DIR))

stage2:
	$(MAKE) -C src/bootloader/stage2 BUILD_DIR=$(abspath $(BUILD_DIR))

kernel:
	$(MAKE) -C src/kernel BUILD_DIR=$(abspath $(BUILD_DIR))

clean:
	rm -rf $(BUILD_DIR)
```

**Component Makefile (Example: stage2/Makefile):**

```makefile
BUILD_DIR ?= $(abspath build)

ASM := nasm
CC := wcc
LD := wlink

SOURCES_C := $(wildcard *.c)
SOURCES_ASM := $(wildcard *.asm)

OBJECTS_C := $(patsubst %.c,$(BUILD_DIR)/stage2/c/%.obj,$(SOURCES_C))
OBJECTS_ASM := $(patsubst %.asm,$(BUILD_DIR)/stage2/asm/%.obj,$(SOURCES_ASM))

all: $(BUILD_DIR)/stage2.bin

$(BUILD_DIR)/stage2.bin: $(OBJECTS_ASM) $(OBJECTS_C)
	$(LD) NAME $@ FILE { $^ } @linker.lnk

$(BUILD_DIR)/stage2/c/%.obj: %.c
	@mkdir -p $(dir $@)
	$(CC) -4 -d3 -s -wx -ms -zl -zq -fo=$@ $<

$(BUILD_DIR)/stage2/asm/%.obj: %.asm
	@mkdir -p $(dir $@)
	$(ASM) -f obj -o $@ $<

clean:
	rm -f $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/stage2/**/*.obj
```

**Key Make Features We Use:**

1. **Pattern Rules**: `%.obj: %.c` matches any .c file
2. **Automatic Variables**: 
   - `$@` = target name
   - `$<` = first prerequisite
   - `$^` = all prerequisites
3. **Wildcard**: `$(wildcard *.c)` finds all .c files
4. **Path Substitution**: `$(patsubst %.c,%.obj,$(SOURCES))` changes extensions
5. **Directory Creation**: `@mkdir -p $(dir $@)` creates output dirs
6. **Subdirectory Makes**: `$(MAKE) -C subdir` builds in subdirectory
7. **Variable Override**: `BUILD_DIR ?= build` allows parent override

#### Disk Image Creation

**Why FAT12 Floppy Image?**

We create a 1.44MB FAT12 floppy image because:

1. **Universal Compatibility**: Works in QEMU, VirtualBox, VMware, real hardware
2. **Simple to Create**: Standard tools (dd, mkfs.fat, mcopy)
3. **Easy to Debug**: Can mount on host OS to inspect
4. **Historical**: Matches early PC boot process

**Image Creation Steps:**

```bash
# 1. Create blank 1.44MB image (2880 sectors × 512 bytes)
dd if=/dev/zero of=floppy.img bs=512 count=2880

# 2. Format as FAT12 filesystem
mkfs.fat -F 12 -n "NBOS" floppy.img

# 3. Install bootloader as boot sector
dd if=stage1.bin of=floppy.img conv=notrunc

# 4. Copy files to filesystem
mcopy -i floppy.img stage2.bin ::/stage2.bin
mcopy -i floppy.img kernel.bin ::/kernel.bin
mcopy -i floppy.img test.txt ::/test.txt
```

**Tool Explanations:**

- **dd**: "Data Duplicator" - low-level copy tool
  - `if=/dev/zero`: Input from null device (creates zeros)
  - `of=floppy.img`: Output to file
  - `bs=512`: Block size 512 bytes
  - `count=2880`: Copy 2880 blocks (1.44MB)
  - `conv=notrunc`: Don't truncate output (preserve FAT12 formatting)

- **mkfs.fat**: Create FAT filesystem
  - `-F 12`: FAT12 type
  - `-n "NBOS"`: Volume label

- **mcopy**: "MTOOLS copy" - copy files to/from FAT images
  - `-i floppy.img`: Image file to operate on
  - `stage2.bin`: Source file (host filesystem)
  - `::/stage2.bin`: Destination (image root directory)
  - Can also use: `mdir`, `mdel`, `mtype`, `mmd`, etc.

**Alternative: Hard Disk Image**

Could use hard disk image instead:

```bash
# Create 10MB hard disk image
dd if=/dev/zero of=hdd.img bs=1M count=10

# Create partition table
fdisk hdd.img <<EOF
n
p
1


a
w
EOF

# Format partition
mkfs.fat -F 16 hdd.img

# Would also need MBR bootloader (more complex)
```

**Why Floppy for Learning:**

- **Simpler**: No partition table, no MBR vs VBR distinction
- **Smaller**: Faster to create and test
- **Historical**: Matches original PC boot process
- **Universal**: Every emulator supports floppy

#### Alternative Build Systems

**1. CMake**

*Modern cross-platform build system*

```cmake
cmake_minimum_required(VERSION 3.10)
project(project_OS ASM C)

add_executable(stage1.bin src/bootloader/stage1/boot.asm)
set_target_properties(stage1.bin PROPERTIES
    LINK_FLAGS "-f bin"
)
```

**Pros:**
- Cross-platform (Windows, Linux, macOS)
- IDE integration (Visual Studio, CLion)
- Dependency tracking
- Out-of-source builds

**Cons:**
- More complex for simple projects
- Learning curve
- Overkill for our use case

**Our choice:** Make is sufficient and more traditional for OS dev

**2. Custom Build Scripts**

*Shell scripts instead of Make*

```bash
#!/bin/bash
nasm -f bin src/bootloader/stage1/boot.asm -o build/stage1.bin
nasm -f obj src/bootloader/stage2/main.asm -o build/main.obj
wcc -4 -d3 -s -wx -ms -zl -zq -fo=build/main.obj src/bootloader/stage2/main.c
# ... etc
```

**Pros:**
- Simple, easy to understand
- No Make knowledge required
- Complete control

**Cons:**
- No dependency tracking (always rebuilds everything)
- No parallel builds
- Platform-specific (bash vs cmd.exe)

**Our choice:** Make provides dependency tracking and parallel builds

**3. Build System Generators (Meson, Bazel, etc.)**

*Modern alternatives to Make*

**Pros:**
- Faster builds
- Better dependency tracking
- Modern features

**Cons:**
- Not standard for OS development
- Less documentation/examples
- Additional dependencies

**Our choice:** Make is standard in OS dev community

### Testing and Debugging Infrastructure

#### Emulators for Testing

**QEMU - Primary Testing Environment**

```bash
# Basic boot test
qemu-system-i386 -fda build/main_floppy.img

# With GDB debugging
qemu-system-i386 -fda build/main_floppy.img -s -S

# With logging
qemu-system-i386 -fda build/main_floppy.img \
    -d int,cpu_reset,guest_errors \
    -D qemu_log.txt
```

**QEMU Advantages:**

- **Fast**: Boots instantly
- **Debuggable**: Built-in GDB stub
- **Feature-rich**: Monitor console for inspection
- **Cross-platform**: Windows, Linux, macOS
- **Free**: Open source

**VirtualBox - Secondary Testing**

```bash
# Create VM
VBoxManage createvm --name "project_OS" --register
VBoxManage modifyvm "project_OS" --memory 128 --boot1 floppy
VBoxManage storagectl "project_OS" --name "Floppy" --add floppy
VBoxManage storageattach "project_OS" --storagectl "Floppy" \
    --port 0 --device 0 --type fdd \
    --medium build/main_floppy.img
```

**VirtualBox Advantages:**

- **Realistic**: Closer to real hardware
- **GUI**: Easy to use
- **Snapshots**: Can save/restore states

**Bochs - Advanced Debugging**

```bash
# Bochs with debugger
bochs -f bochsrc.txt -q
```

**Bochs Advantages:**

- **Best Debugger**: Built-in instruction-level debugger
- **Inspection**: Can examine all CPU state
- **Breakpoints**: Hardware and software breakpoints

**Why Test in Multiple Emulators?**

Different emulators have different quirks:
- QEMU: Sometimes too forgiving
- VirtualBox: More realistic CPU behavior
- Bochs: Strict, catches more bugs

Testing in all three ensures compatibility.

#### Debugging Tools

**1. GDB with QEMU**

```bash
# Terminal 1: Start QEMU with GDB server
qemu-system-i386 -fda build/main_floppy.img -s -S

# Terminal 2: Connect GDB
gdb
(gdb) target remote localhost:1234
(gdb) set architecture i8086
(gdb) break *0x7c00
(gdb) continue
(gdb) info registers
(gdb) x/32xb 0x7c00
```

**2. Hexdump for Binary Inspection**

```bash
# View boot sector
hexdump -C build/stage1.bin | head -n 32

# Check boot signature (last 2 bytes should be 55 AA)
tail -c 2 build/stage1.bin | hexdump -C
```

**3. Objdump for Disassembly**

```bash
# Disassemble 32-bit kernel
objdump -D -b binary -m i386 -M intel build/kernel.bin | less

# Disassemble ELF object
objdump -d build/kernel.o
```

**4. MTOOLS for Filesystem Inspection**

```bash
# List files in image
mdir -i build/main_floppy.img ::/

# View file contents
mtype -i build/main_floppy.img ::/test.txt

# Extract file from image
mcopy -i build/main_floppy.img ::/stage2.bin ./extracted.bin

# Filesystem info
minfo -i build/main_floppy.img
```

**5. strace/ltrace for Tool Debugging**

```bash
# Trace system calls made by build tools
strace -e open,read,write wcc main.c

# Trace library calls
ltrace wlink NAME stage2.bin FILE main.obj
```

### Build Performance Optimization

**Current Build Times (Approximate):**

```
Component         Time      Size Output
=============================================
Stage 1          0.1s      512 B
Stage 2 (asm)    0.1s      ~100 B
Stage 2 (C)      0.3s      ~2 KB
Stage 2 (link)   0.2s      ~2 KB total
Kernel (asm)     0.1s      ~50 B
Kernel (C)       0.4s      ~1 KB
Kernel (link)    0.2s      ~1 KB total
Disk image       0.5s      1.44 MB
Total (clean)    ~2s
Total (incremental) ~0.5s
```

**Optimization Strategies:**

1. **Parallel Builds**: `make -j4` (4 jobs)
2. **Incremental Builds**: Only rebuild changed files
3. **Separate Objects**: Compile each .c file separately
4. **Build Directory**: All outputs in build/, source unchanged
5. **Dependency Tracking**: Make knows what needs rebuilding

**Scaling for Larger Projects:**

As project grows, build time management becomes critical:

- **Precompiled Headers**: Not applicable (bare metal)
- **Unity Builds**: Could combine .c files (trades compile time for link time)
- **Distributed Builds**: distcc for very large projects
- **ccache**: Compiler cache (helps with clean builds)

For now, build is fast enough that optimization isn't needed.

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
  - ✅ Stage 2 has error handling (printf)
  - ✅ Diagnostic output available

**❌ Not Yet Implemented:**
- ❌ Memory management (paging, heap)
- ❌ Interrupt Descriptor Table (IDT) setup
- ❌ Device drivers (keyboard, disk, etc.)
- ❌ Any actual OS functionality
- ❌ User mode vs kernel mode
- ❌ System calls
- ❌ Multitasking
- ❌ File system write support

### Next Steps (Priority Order)

**Immediate Next Steps (Kernel Enhancement):**
1. **Setup Interrupt Descriptor Table (IDT)**
   - Handle CPU exceptions
   - Remap PIC (Programmable Interrupt Controller)
   - Enable hardware interrupts

2. **Memory Management**
   - Implement Physical Memory Manager (PMM)
   - Implement Virtual Memory Manager (VMM) / Paging

3. **Keyboard Driver**
   - Implement PS/2 keyboard driver
   - Handle keyboard interrupts

**Completed Goals:**
- ✅ Implement kernel loading in Stage 2
- ✅ Add error handling to Stage 2
- ✅ Protected Mode Transition
- ✅ Basic VGA text output in Kernel

**Medium-term Goals:**
1. Basic shell/command interpreter
2. File system read support in Kernel
3. Multitasking support

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

**Bootloader: 16-bit Real Mode**

The bootloader (Stage 1 & 2) runs in real mode (8086 compatible):

**Characteristics:**
- Segmented memory model: `address = segment × 16 + offset`
- 1 MB address space (20-bit addressing)
- Only 640 KB usable (upper 384 KB reserved for hardware)
- No memory protection (any code can access any memory)
- BIOS interrupts available (used for disk I/O and screen output)

**Kernel: 32-bit Protected Mode**

The kernel runs in 32-bit protected mode:

**Characteristics:**
- Flat memory model: 4 GB address space
- Memory protection (segments can be read-only, execute-only, etc.)
- Privilege levels (Ring 0-3): kernel vs user code
- Virtual memory support (paging)
- Interrupt Descriptor Table (IDT) replaces IVT

**Transition process (Implemented in Stage 2):**
1. Disable interrupts
2. Load Global Descriptor Table (GDT)
3. Enable A20 line (access above 1 MB)
4. Set PE bit in CR0 register
5. Far jump to 32-bit code segment
6. Setup new segment registers
7. Jump to Kernel Entry Point

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

## Implementation Deep-Dive: Code Walkthrough

This section provides granular, line-by-line explanations of how our OS implementation works. It covers the actual code patterns, register usage, and low-level details that make everything function.

### Stage 1 Bootloader: Complete Code Analysis

Our Stage 1 bootloader (`src/bootloader/stage1/boot.asm`) is exactly 512 bytes and implements FAT12 filesystem reading. Here's how each part works:

#### FAT12 BIOS Parameter Block (BPB)

```assembly
; Lines 8-36: FAT12 Header Structure
; This MUST be at the beginning of the boot sector for BIOS/filesystem compatibility

jmp short start        ; 2 bytes: Jump over BPB (EB xx)
nop                    ; 1 byte: NOP padding (90)

bdb_oem:                    db 'MSWIN4.1'           ; OEM identifier (8 bytes)
bdb_bytes_per_sector:       dw 512                  ; ALWAYS 512 for floppies
bdb_sectors_per_cluster:    db 1                    ; 1 sector = 1 cluster (simplest)
bdb_reserved_sectors:       dw 1                    ; Just the boot sector
bdb_fat_count:              db 2                    ; Two FAT copies for redundancy
bdb_dir_entries_count:      dw 0E0h                 ; 224 entries (14 sectors worth)
bdb_total_sectors:          dw 2880                 ; 1.44 MB floppy = 2880 sectors
bdb_media_descriptor_type:  db 0F0h                 ; 0xF0 = 3.5" floppy
bdb_sectors_per_fat:        dw 9                    ; Each FAT is 9 sectors
bdb_sectors_per_track:      dw 18                   ; 18 sectors per track (standard)
bdb_heads:                  dw 2                    ; 2 heads (double-sided)
bdb_hidden_sectors:         dd 0                    ; No hidden sectors
bdb_large_sector_count:     dd 0                    ; Not used for small disks
```

**Why these values matter:**
- The BPB tells both BIOS and our code how the disk is formatted
- Changing `bdb_bytes_per_sector` from 512 would break everything
- `bdb_dir_entries_count` of 224 allows 224 files in root directory
- Each directory entry is 32 bytes, so root dir = 224 × 32 = 7168 bytes = 14 sectors

#### Segment and Stack Initialization

```assembly
; Lines 38-51: Hardware initialization
start:
    mov ax, 0           ; Can't directly write to segment registers
    mov ds, ax          ; DS = 0 (data segment)
    mov es, ax          ; ES = 0 (extra segment)
    mov ss, ax          ; SS = 0 (stack segment)
    mov sp, 0x7C00      ; Stack starts at bootloader address, grows DOWN

    ; Far jump trick to set CS to 0
    push es             ; Push segment 0
    push word .after    ; Push offset of .after
    retf                ; Far return = sets CS:IP
.after:
```

**Why `retf` for CS?**
- You cannot directly `mov` to CS register
- `retf` pops IP then CS from stack
- This ensures CS=0 matching DS, ES, SS for consistent addressing
- All our addresses will be `0:offset` format

#### Disk Geometry Detection

```assembly
; Lines 61-72: Get disk parameters from BIOS
    mov [ebr_drive_number], dl   ; DL contains boot drive (passed by BIOS)
    
    push es
    mov ah, 08h                  ; BIOS function: Get drive parameters
    int 13h                      ; Call BIOS disk interrupt
    jc floppy_error_params       ; Jump if carry flag set (error)
    pop es
    
    and cl, 0x3F                 ; Extract sectors (bits 0-5)
    xor ch, ch                   ; Clear high byte
    mov [bdb_sectors_per_track], cx  ; Save sectors per track
    
    inc dh                       ; DH = number of heads - 1, so add 1
    mov [bdb_heads], dh          ; Save number of heads
```

**Register usage after INT 13h, AH=08h:**
- DL = drive number (unchanged)
- DH = number of heads - 1
- CL bits 5-0 = sectors per track
- CL bits 7-6 + CH = cylinders - 1
- Carry flag = error status

#### Root Directory Loading

```assembly
; Lines 74-95: Calculate and load root directory
    ; Root directory LBA = reserved_sectors + (fat_count × sectors_per_fat)
    mov ax, [bdb_sectors_per_fat]    ; AX = 9 (sectors per FAT)
    mov bl, [bdb_fat_count]          ; BL = 2 (number of FATs)
    xor bh, bh                       ; Clear high byte
    mul bx                           ; AX = 9 × 2 = 18
    add ax, [bdb_reserved_sectors]   ; AX = 18 + 1 = 19 (root dir LBA)
    push ax                          ; Save root dir LBA for later
    
    ; Root directory size = (32 × dir_entries) / bytes_per_sector
    mov ax, [bdb_dir_entries_count]  ; AX = 224
    shl ax, 5                        ; AX = 224 × 32 = 7168 (shift left 5 = × 32)
    xor dx, dx                       ; Clear for division
    div word [bdb_bytes_per_sector]  ; AX = 7168 / 512 = 14 sectors
    
    mov cl, al                       ; CL = 14 (sectors to read)
    pop ax                           ; AX = 19 (root dir LBA)
    mov dl, [ebr_drive_number]       ; DL = boot drive
    mov bx, buffer                   ; ES:BX = buffer address
    call disk_read                   ; Read root directory
```

**Memory layout after this:**
- `buffer` (at 0x7E00) contains 14 sectors of root directory
- Each sector = 512 bytes, so buffer contains 7168 bytes
- This holds all 224 directory entries (32 bytes each)

#### FAT12 Cluster Chain Following

```assembly
; Lines 175-200: FAT12 cluster chain traversal
    ; FAT12 uses 12-bit cluster numbers packed into bytes
    ; Even clusters: low 12 bits of word at (cluster × 3 / 2)
    ; Odd clusters: high 12 bits of word at (cluster × 3 / 2)
    
    mov ax, [stage2_cluster]    ; Current cluster number
    mov cx, 3
    mul cx                      ; AX = cluster × 3
    mov cx, 2
    div cx                      ; AX = (cluster × 3) / 2
                                ; DX = remainder (0 = even, 1 = odd)
    
    mov si, buffer              ; SI = FAT buffer start
    add si, ax                  ; SI = FAT buffer + offset
    mov ax, [ds:si]             ; Load 16-bit word from FAT
    
    or dx, dx                   ; Check if even or odd cluster
    jz .even
    
.odd:
    shr ax, 4                   ; Odd: shift right 4 bits (use high 12 bits)
    jmp .next_cluster_after
    
.even:
    and ax, 0x0FFF              ; Even: mask to low 12 bits
    
.next_cluster_after:
    cmp ax, 0x0FF8              ; Check for end-of-chain marker
    jae .read_finish            ; >= 0xFF8 means EOF
    
    mov [stage2_cluster], ax    ; Save next cluster
    jmp .load_stage2_loop       ; Continue loading
```

**FAT12 Cluster Packing Example:**
```
Byte offset:  0   1   2   3   4   5
Binary:      [AB][CD][EF][GH][IJ][KL]

Cluster 0 (even): 0x0DAB (bytes 0-1, low 12 bits)
Cluster 1 (odd):  0x0EFC (bytes 1-2, high 12 bits, shifted right 4)
Cluster 2 (even): 0x0GHE (bytes 3-4, low 12 bits)
...and so on
```

#### LBA to CHS Conversion

```assembly
; Lines 247-269: Convert Logical Block Address to Cylinder/Head/Sector
lba_to_chs:
    push ax
    push dx
    
    ; Sector = (LBA % sectors_per_track) + 1
    xor dx, dx                              ; Clear DX for division
    div word [bdb_sectors_per_track]        ; AX = LBA / SPT, DX = LBA % SPT
    inc dx                                  ; Sectors are 1-indexed!
    mov cx, dx                              ; CX = sector (bits 0-5)
    
    ; Head and Cylinder
    xor dx, dx                              ; Clear DX
    div word [bdb_heads]                    ; AX = temp / heads = cylinder
                                            ; DX = temp % heads = head
    mov dh, dl                              ; DH = head
    mov ch, al                              ; CH = cylinder low 8 bits
    shl ah, 6                               ; AH bits 0-1 become bits 6-7
    or cl, ah                               ; CL = sector + cylinder high 2 bits
    
    pop ax
    mov dl, al                              ; Restore DL (drive number)
    pop ax
    ret
```

**CHS Format for INT 13h:**
- CH = cylinder low 8 bits
- CL bits 0-5 = sector (1-63)
- CL bits 6-7 = cylinder high 2 bits
- DH = head (0-255)
- DL = drive number

### Stage 2 Bootloader: C/Assembly Integration

Stage 2 demonstrates how to integrate C code with bare-metal assembly.

#### Assembly Entry Point Analysis

```assembly
; src/bootloader/stage2/main.asm - Entry point
bits 16
section _ENTRY class=CODE

extern _cstart_     ; C function (Open Watcom adds underscore prefix)
global entry

entry:
    cli             ; Disable interrupts during critical setup
    
    ; Setup segments - DS already set by Stage 1
    mov ax, ds      ; Copy DS to AX
    mov ss, ax      ; SS = DS (same segment for stack)
    mov sp, 0       ; SP = 0, stack grows DOWN from 0xFFFF
    mov bp, sp      ; BP = 0 (base pointer for stack frames)
    
    sti             ; Re-enable interrupts
    
    ; Prepare C function call
    xor dh, dh      ; Clear DH (DL contains boot drive from Stage 1)
    push dx         ; Push boot drive as 16-bit parameter
    call _cstart_   ; Call C entry point
    
    ; If C returns (shouldn't happen), halt
    cli
    hlt
```

**Open Watcom Calling Convention:**
1. Parameters pushed right-to-left onto stack
2. Caller cleans up stack after call
3. Return value in AX (16-bit) or DX:AX (32-bit)
4. All C function names prefixed with underscore

#### C Entry Point Implementation

```c
// src/bootloader/stage2/main.c

#define _cdecl __attribute__((cdecl))

void _cdecl cstart_(uint16_t bootDrive) {
    // bootDrive parameter is on stack at [BP+4]
    
    // Initialize disk subsystem
    DISK disk;
    if (!DISK_Initialize(&disk, (uint8_t)bootDrive)) {
        printf("[ERR] Could not initialize disk!\r\n");
        goto end;
    }
    
    // Initialize FAT filesystem
    if (!FAT_Initialize(&disk)) {
        printf("[ERR] Could not initialize FAT!\r\n");
        goto end;
    }
    
    // Load kernel from filesystem
    FAT_File *fd = FAT_Open(&disk, "/kernel.bin");
    if (fd == NULL) {
        printf("[ERR] kernel.bin not found!\r\n");
        goto end;
    }
    
    // Read kernel to temporary buffer
    uint8_t *kernelBuffer = (uint8_t *)0x30000;
    uint32_t read = FAT_Read(&disk, fd, fd->Size, kernelBuffer);
    FAT_Close(fd);
    
    // Copy to high memory (1MB mark)
    uint8_t *kernelStart = (uint8_t *)0x100000;
    memcpy(kernelStart, kernelBuffer, read);
    
    // Jump to kernel entry point
    // (Protected mode transition handled by inline assembly)
    
end:
    for (;;);  // Halt on error
}
```

#### BIOS Interrupt Wrapper

```assembly
; src/bootloader/stage2/x86.asm - BIOS INT 10h wrapper

global _x86_Video_WriteCharTeletype
_x86_Video_WriteCharTeletype:
    ; Function signature: void x86_Video_WriteCharTeletype(char c, uint8_t page)
    ; Stack on entry:
    ;   [BP+0] = saved BP
    ;   [BP+2] = return address
    ;   [BP+4] = character (uint8_t, stored as word)
    ;   [BP+6] = page (uint8_t, stored as word)
    
    push bp
    mov bp, sp
    
    mov ah, 0x0E            ; BIOS teletype function
    mov al, [bp + 4]        ; Character to print
    mov bh, [bp + 6]        ; Page number (usually 0)
    
    int 0x10                ; Call BIOS video interrupt
    
    mov sp, bp
    pop bp
    ret
```

### Kernel: 64-bit Long Mode Implementation

Our kernel transitions from 32-bit protected mode to 64-bit long mode.

#### Multiboot Header

```assembly
; src/kernel/entry.asm - Multiboot header
section .multiboot
align 4

MULTIBOOT_MAGIC        equ 0x1BADB002
MULTIBOOT_PAGE_ALIGN   equ 1 << 0
MULTIBOOT_MEMORY_INFO  equ 1 << 1
MULTIBOOT_FLAGS        equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO
MULTIBOOT_CHECKSUM     equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

multiboot_header:
    dd MULTIBOOT_MAGIC      ; Magic number
    dd MULTIBOOT_FLAGS      ; Flags
    dd MULTIBOOT_CHECKSUM   ; Checksum (must sum to 0)
```

**Multiboot Protocol:**
- GRUB/our bootloader passes magic in EAX (0x2BADB002)
- Info structure pointer in EBX
- Allows booting by any Multiboot-compliant loader

#### Long Mode Transition

```assembly
; Lines 37-156: Complete long mode setup

_start:
    cli                     ; Disable interrupts
    
    ; Save multiboot info
    mov edi, eax           ; Save magic in EDI (preserved to RDI in 64-bit)
    mov esi, ebx           ; Save info pointer in ESI (preserved to RSI)
    
    ; Set up temporary 32-bit stack
    mov esp, stack_top
    mov ebp, esp
    
    ; Check for Long Mode support
    mov eax, 0x80000001    ; Extended CPUID
    cpuid
    test edx, 1 << 29      ; Check LM bit (bit 29 of EDX)
    jz .no_long_mode       ; Jump if Long Mode not supported
    
    ; === Set up 4-level paging ===
    
    ; Clear page tables (16KB at pml4_table)
    mov edi, pml4_table
    xor eax, eax
    mov ecx, 4096          ; 16KB = 4096 dwords
    rep stosd
    
    ; PML4[0] -> PDPT
    mov eax, pdpt_table
    or eax, PAGE_PRESENT | PAGE_WRITABLE   ; 0x03
    mov [pml4_table], eax
    
    ; PDPT[0] -> PD (using 2MB huge pages for simplicity)
    mov eax, pd_table
    or eax, PAGE_PRESENT | PAGE_WRITABLE
    mov [pdpt_table], eax
    
    ; Fill Page Directory with 2MB pages (identity mapping first 4GB)
    mov edi, pd_table
    mov eax, PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE  ; 0x83
    mov ecx, 2048          ; 2048 × 2MB = 4GB
.map_pd:
    mov [edi], eax
    add eax, 0x200000      ; Next 2MB physical address
    add edi, 8             ; Next PD entry (8 bytes in 64-bit mode)
    loop .map_pd
    
    ; === Enable PAE ===
    mov eax, cr4
    or eax, 1 << 5         ; PAE bit
    mov cr4, eax
    
    ; === Load PML4 into CR3 ===
    mov eax, pml4_table
    mov cr3, eax
    
    ; === Enable Long Mode ===
    mov ecx, 0xC0000080    ; EFER MSR
    rdmsr
    or eax, 1 << 8         ; LME bit
    wrmsr
    
    ; === Enable Paging (activates Long Mode) ===
    mov eax, cr0
    or eax, 1 << 31        ; PG bit
    mov cr0, eax
    
    ; === Far jump to 64-bit code ===
    lgdt [gdt64_ptr]
    jmp 0x08:long_mode_start   ; CS = 0x08 (64-bit code segment)
```

**Page Table Structure (4-level):**
```
PML4 (Page Map Level 4) - 512 entries × 8 bytes = 4KB
  └── PDPT (Page Directory Pointer Table) - 512 entries × 8 bytes = 4KB
       └── PD (Page Directory) - 512 entries × 8 bytes = 4KB
            └── PT (Page Table) - 512 entries × 8 bytes = 4KB
                 └── Physical page (4KB) or 2MB huge page

We use 2MB huge pages for simplicity:
- PD entry with PAGE_HUGE bit maps 2MB directly
- No PT level needed
- 512 PD entries × 2MB = 1GB per PDPT entry
- 4 PDPT entries = 4GB total (enough for our kernel + framebuffer)
```

### Interrupt Handling Implementation

#### IDT Structure (64-bit)

```c
// src/kernel/idt.h
typedef struct {
    uint16_t base_low;      // Offset bits 0-15
    uint16_t sel;           // Code segment selector (0x08)
    uint8_t  ist;           // Interrupt Stack Table (0 = disabled)
    uint8_t  flags;         // Type and attributes
    uint16_t base_mid;      // Offset bits 16-31
    uint32_t base_high;     // Offset bits 32-63
    uint32_t reserved;      // Must be 0
} __attribute__((packed)) IDTEntry;  // 16 bytes per entry

// IDT has 256 entries (0-255)
IDTEntry idt[256];
```

**IDT Entry Flags (byte 5):**
```
Bit 7: Present (P) - Must be 1 for valid entry
Bit 6-5: DPL - Descriptor Privilege Level (0 = kernel)
Bit 4: 0 (always)
Bit 3-0: Gate Type
  - 0xE = 64-bit Interrupt Gate (clears IF flag)
  - 0xF = 64-bit Trap Gate (doesn't clear IF flag)

For interrupt handlers: flags = 0x8E (Present, DPL=0, Interrupt Gate)
```

#### PIC Remapping

```c
// src/kernel/i8259.c - Programmable Interrupt Controller setup

void i8259_init() {
    // Save current masks
    unsigned char a1 = inb(PIC1_DATA);  // Port 0x21
    unsigned char a2 = inb(PIC2_DATA);  // Port 0xA1
    
    // ICW1: Initialize + expect ICW4
    outb(PIC1_COMMAND, 0x11);  // Port 0x20
    io_wait();
    outb(PIC2_COMMAND, 0x11);  // Port 0xA0
    io_wait();
    
    // ICW2: Vector offsets
    outb(PIC1_DATA, 0x20);     // Master PIC: IRQs 0-7 → INT 0x20-0x27
    io_wait();
    outb(PIC2_DATA, 0x28);     // Slave PIC: IRQs 8-15 → INT 0x28-0x2F
    io_wait();
    
    // ICW3: Cascade configuration
    outb(PIC1_DATA, 0x04);     // Master: slave on IRQ2 (bit 2)
    io_wait();
    outb(PIC2_DATA, 0x02);     // Slave: cascade identity (IRQ2)
    io_wait();
    
    // ICW4: 8086 mode
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();
    
    // Restore masks
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}
```

**Why remap?**
- Default PIC maps IRQ 0-7 to INT 0x08-0x0F
- Intel reserved INT 0x00-0x1F for CPU exceptions (e.g., #DF Double Fault = 0x08, #GP General Protection = 0x0D)
- Conflict: Hardware IRQs would clash with CPU exception handlers
- Solution: Remap PIC to start at 0x20 (avoiding reserved exception range)

### Keyboard Driver Implementation

```c
// src/kernel/keyboard.c

// US QWERTY scancode set 1 to ASCII mapping
static const char scancode_to_ascii[] = {
    0,    0,    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-',  '=',
    '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[',  ']',
    '\n', 0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    // ... continues for all keys
};

// Keyboard interrupt handler (IRQ 1 = INT 0x21)
void keyboard_handler(Registers *regs) {
    uint8_t scancode = inb(0x60);   // Read scancode from keyboard data port
    
    // Key release: bit 7 is set
    if (scancode & 0x80) {
        scancode &= 0x7F;           // Clear release bit
        if (scancode == 0x2A || scancode == 0x36) {  // Left/Right Shift
            shift_pressed = 0;
        }
        return;
    }
    
    // Key press
    if (scancode == 0x2A || scancode == 0x36) {  // Shift keys
        shift_pressed = 1;
        return;
    }
    
    // Convert scancode to ASCII
    char c = shift_pressed ? scancode_to_ascii_shift[scancode]
                           : scancode_to_ascii[scancode];
    
    if (c != 0) {
        key_buffer_push(c);    // Add to circular buffer
        shell_putchar(c);      // Echo to shell
    }
}

// Register handler during init
void keyboard_init(void) {
    // After PIC remapping: IRQ 0-7 → INT 0x20-0x27 (32-39 decimal)
    // Keyboard is IRQ 1, so it maps to INT 0x21 = 33 decimal
    register_interrupt_handler(33, keyboard_handler);
}
```

**Keyboard I/O Ports:**
- Port 0x60: Data port (read scancodes)
- Port 0x64: Status/command port
  - Reading: bit 0 = output buffer full (data ready)
  - Writing: send commands to keyboard controller

### VGA Text Mode Implementation

```c
// src/kernel/main.c - Direct VGA memory access

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

// VGA text buffer: 80×25 characters, 2 bytes each
// Byte 0: ASCII character
// Byte 1: Attribute (foreground 4 bits | background 4 bits)
static volatile uint8_t *video_memory = (volatile uint8_t *)VGA_MEMORY;

void putchar_at(char c, uint8_t color, int col, int row) {
    int offset = (row * VGA_WIDTH + col) * 2;
    video_memory[offset] = c;           // Character
    video_memory[offset + 1] = color;   // Attribute
}

void scroll_screen(void) {
    // Copy lines 1-24 to lines 0-23
    for (int row = 1; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH * 2; col++) {
            int src = (row * VGA_WIDTH * 2) + col;
            int dst = ((row - 1) * VGA_WIDTH * 2) + col;
            video_memory[dst] = video_memory[src];
        }
    }
    
    // Clear last line
    for (int col = 0; col < VGA_WIDTH; col++) {
        int offset = (VGA_HEIGHT - 1) * VGA_WIDTH * 2 + col * 2;
        video_memory[offset] = ' ';
        video_memory[offset + 1] = 0x07;  // Light gray on black
    }
}
```

**VGA Color Attributes:**
```
Bit 7: Blink (or bright background if disabled)
Bits 6-4: Background color (0-7)
Bit 3: Bright foreground
Bits 2-0: Foreground color (0-7)

Colors:
0 = Black    4 = Red
1 = Blue     5 = Magenta
2 = Green    6 = Brown
3 = Cyan     7 = Light Gray

Example: 0x1F = White on Blue (0001 1111)
         0x4E = Yellow on Red (0100 1110)
```

### FAT12 Implementation Details

```c
// src/bootloader/stage2/fat.c

// FAT12 cluster chain reading
uint32_t FAT_NextCluster(uint32_t currentCluster) {
    // FAT12: 12 bits per entry, packed into bytes
    // Entry at byte offset (cluster × 3 / 2)
    
    uint32_t fatIndex = currentCluster * 3 / 2;
    
    if (currentCluster % 2 == 0) {
        // Even cluster: take low 12 bits
        return (*(uint16_t *)(g_Fat + fatIndex)) & 0x0FFF;
    } else {
        // Odd cluster: take high 12 bits (shift right 4)
        return (*(uint16_t *)(g_Fat + fatIndex)) >> 4;
    }
}

// Convert cluster number to LBA (sector address)
uint32_t FAT_ClusterToLba(uint32_t cluster) {
    // Data area starts after: reserved + FATs + root directory
    // First data cluster is #2 (clusters 0 and 1 are reserved)
    return g_DataSectionLba + (cluster - 2) * g_Data->BS.BootSector.SectorsPerCluster;
}
```

**FAT12 Disk Layout:**
```
Sector 0:     Boot sector (with BPB)
Sectors 1-9:  FAT #1 (9 sectors × 512 bytes = 4608 bytes)
Sectors 10-18: FAT #2 (backup copy)
Sectors 19-32: Root directory (14 sectors × 512 = 7168 bytes = 224 entries)
Sectors 33+:  Data area (file contents)

Cluster → Sector formula:
  LBA = 33 + (cluster - 2) × 1
      = 33 + cluster - 2
      = 31 + cluster
```

### Shell Implementation

```c
// src/kernel/shell.c - Interactive command shell

void execute_command(void) {
    // Parse command and arguments
    char *cmd = input_buffer;
    char *args = input_buffer;
    
    // Find first space (separates command from arguments)
    while (*args && *args != ' ') args++;
    if (*args) {
        *args = '\0';  // Null-terminate command
        args++;        // Point to arguments
        while (*args == ' ') args++;  // Skip leading spaces
    }
    
    // Command dispatch
    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0) {
        clear_screen(0x07);
    } else if (strcmp(cmd, "reboot") == 0) {
        // Triple-fault to reboot
        outb(0x64, 0xFE);  // Pulse CPU reset line via keyboard controller
    } else if (strcmp(cmd, "peek") == 0) {
        // Memory inspection: peek <hex_address>
        uint64_t addr = atoi_hex(args);
        uint8_t *ptr = (uint8_t *)addr;
        // Print 16 bytes at address
        for (int i = 0; i < 16; i++) {
            print_hex_byte(ptr[i]);
        }
    }
    // ... more commands ...
}
```

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

## How to Extend This OS

This section provides practical guidance on how to add new features to the OS.

### Adding a New Function to Stage 2

**Example: Adding a function to print a hexadecimal byte**

1. **Add declaration to header** (`stdio.h`):
   ```c
   void put_hex_byte(uint8_t value);
   ```

2. **Implement in C** (`stdio.c`):
   ```c
   void put_hex_byte(uint8_t value)
   {
       const char hex_chars[] = "0123456789ABCDEF";
       putc(hex_chars[value >> 4]);    // High nibble
       putc(hex_chars[value & 0x0F]);  // Low nibble
   }
   ```

3. **Use in main.c:**
   ```c
   void _cdecl cstart_(uint16_t bootDrive)
   {
       puts("Boot drive: 0x");
       put_hex_byte((uint8_t)bootDrive);
       puts("\r\n");
       for(;;);
   }
   ```

4. **Rebuild and test:**
   ```bash
   make clean
   make
   ./run.sh
   ```

### Adding a New BIOS Interrupt Wrapper

**Example: Adding keyboard input support (INT 16h)**

1. **Add declaration to x86.h:**
   ```c
   uint16_t _cdecl x86_Keyboard_WaitAndRead(void);
   ```

2. **Implement in x86.asm:**
   ```assembly
   global _x86_Keyboard_WaitAndRead
   _x86_Keyboard_WaitAndRead:
       push bp
       mov bp, sp
       
       xor ah, ah        ; Function 0: Wait for keystroke
       int 16h           ; BIOS keyboard interrupt
                         ; AL = ASCII character
                         ; AH = Scan code
       
       mov sp, bp
       pop bp
       ret
   ```

3. **Use in main.c:**
   ```c
   #include "x86.h"
   
   void _cdecl cstart_(uint16_t bootDrive)
   {
       puts("Press any key...\r\n");
       uint16_t key = x86_Keyboard_WaitAndRead();
       puts("You pressed a key!\r\n");
       for(;;);
   }
   ```

### Implementing Kernel Loading in Stage 2

To make Stage 2 actually load and execute the kernel:

1. **Add FAT12 reading code to Stage 2** (in C):
   - Port the FAT12 reading logic from Stage 1 to C
   - Or implement a simpler version using BIOS INT 13h

2. **Example skeleton:**
   ```c
   // In main.c
   void _cdecl cstart_(uint16_t bootDrive)
   {
       puts("Stage 2 bootloader starting...\r\n");
       
       // TODO: Read FAT
       // TODO: Find kernel.bin
       // TODO: Load kernel to 0x2000:0x0000
       
       puts("Loading kernel...\r\n");
       // load_kernel(bootDrive);
       
       puts("Jumping to kernel...\r\n");
       // jump_to_kernel();
       
       for(;;);
   }
   ```

3. **Far jump to kernel:**
   ```c
   // Need assembly helper for far jump
   // In x86.asm:
   global _x86_FarJump
   _x86_FarJump:
       push bp
       mov bp, sp
       
       mov ax, [bp + 4]    ; Segment
       mov bx, [bp + 6]    ; Offset
       
       push ax
       push bx
       retf                ; Far return = far jump
   
   // In x86.h:
   void _cdecl x86_FarJump(uint16_t segment, uint16_t offset);
   
   // In main.c:
   x86_FarJump(0x2000, 0x0000);  // Jump to kernel
   ```

### Transitioning to Protected Mode

**High-level steps:**

1. **Setup GDT (Global Descriptor Table):**
   ```c
   // Define GDT entries (code and data segments)
   struct GDTEntry {
       uint16_t limit_low;
       uint16_t base_low;
       uint8_t base_mid;
       uint8_t access;
       uint8_t granularity;
       uint8_t base_high;
   } __attribute__((packed));
   
   struct GDTEntry gdt[3];  // Null, code, data
   // Initialize entries...
   ```

2. **Load GDT:**
   ```assembly
   lgdt [gdt_descriptor]
   ```

3. **Enable A20 line:**
   ```assembly
   in al, 0x92
   or al, 2
   out 0x92, al
   ```

4. **Set PE bit in CR0:**
   ```assembly
   mov eax, cr0
   or eax, 1
   mov cr0, eax
   ```

5. **Far jump to 32-bit code:**
   ```assembly
   jmp 0x08:protected_mode_entry  ; 0x08 = code segment
   ```

6. **Setup segment registers for protected mode:**
   ```assembly
   bits 32
   protected_mode_entry:
       mov ax, 0x10      ; Data segment
       mov ds, ax
       mov es, ax
       mov fs, ax
       mov gs, ax
       mov ss, ax
       mov esp, 0x90000  ; New stack
       ; Now in 32-bit protected mode!
   ```

### Adding More Features

**Implement a simple command shell:**
1. Add keyboard input functions
2. Implement string comparison functions
3. Create command parser
4. Execute commands (help, clear, reboot, etc.)

**Implement VGA text mode driver:**
1. Replace BIOS calls with direct VGA memory writes
2. Memory-mapped at 0xB8000
3. Format: [char, attribute] pairs
4. 80x25 characters = 4000 bytes

**Add timer support:**
1. Program PIT (Programmable Interval Timer)
2. Setup IRQ 0 handler
3. Implement simple scheduler

### Testing Your Changes

**Always test after each change:**

1. **Build:**
   ```bash
   make clean && make
   ```

2. **Check build succeeded:**
   ```bash
   ls -lh build/stage2.bin build/main_floppy.img
   ```

3. **Test in QEMU:**
   ```bash
   ./run.sh
   ```

4. **If something breaks:**
   ```bash
   # Check linker map
   cat build/stage2.map
   
   # Disassemble Stage 2
   ndisasm -b 16 build/stage2.bin | head -n 20
   
   # Check disk image contents
   mdir -i build/main_floppy.img ::/
   
   # Debug with GDB
   qemu-system-i386 -fda build/main_floppy.img -s -S &
   gdb
   (gdb) target remote localhost:1234
   (gdb) set architecture i8086
   (gdb) break *0x500
   (gdb) continue
   (gdb) layout asm
   (gdb) stepi
   ```

### Best Practices

**When modifying code:**
1. Make one change at a time
2. Test after each change
3. Keep backups of working versions
4. Use git commits frequently
5. Document your changes

**When adding features:**
1. Start simple (print a message)
2. Test thoroughly
3. Add error handling
4. Consider memory constraints
5. Think about side effects

**Memory management tips:**
1. Stage 2 has ~29 KB available (0x0500-0x7BFF)
2. Use static variables sparingly
3. No malloc/free available (would need to implement)
4. Be careful with stack usage
5. Consider segment limits

**Code organization:**
1. Put hardware-specific code in x86.asm
2. Put high-level logic in C files
3. Keep headers minimal
4. Group related functions together
5. Use meaningful names

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

## Design Philosophy and Lessons Learned

### Why These Design Choices Matter

This project embodies several key principles of OS development:

**1. Incremental Complexity**

We deliberately chose to:
- Start with 16-bit real mode (simpler, BIOS available)
- Move to 32-bit protected mode (memory protection, larger address space)
- Eventually target 64-bit long mode (modern hardware features)

**Rationale:** Each step builds on previous knowledge. Jumping straight to 64-bit would skip fundamental concepts like segmentation, real mode limitations, and BIOS programming.

**2. Learning Over Efficiency**

We chose FAT12 over more advanced filesystems because:
- **Simplicity**: Can implement in ~200 lines
- **Debuggability**: Easy to inspect with hex editor
- **Documentation**: Well-specified by Microsoft
- **Universality**: Works on all host operating systems

**Trade-off:** FAT12 is limited (32MB max, no journaling, fragmentation issues). But these limitations are educational - they show why modern filesystems are complex.

**3. Toolchain Pragmatism**

Why multiple assemblers and compilers?
- **NASM for assembly**: Industry standard for OS dev, clear syntax
- **Open Watcom for 16-bit C**: One of few compilers still supporting real mode
- **GCC for 32-bit C**: Universal, well-documented, excellent optimizer

**Lesson:** Real OS development requires understanding different toolchains and their strengths.

**4. Modularity From Day One**

Separate Makefiles for each component because:
- **Isolation**: Kernel changes don't trigger bootloader rebuilds
- **Clarity**: Each component has focused, understandable build rules
- **Scalability**: Easy to add new components (drivers, utilities)
- **Parallel Building**: `make -j4` speeds up development

**Lesson:** Good build system design pays dividends as project grows.

### What This Project Teaches

**Low-Level Programming Concepts:**

1. **Memory is Just Addresses**: Everything is bytes at specific locations
2. **Calling Conventions Matter**: How C and assembly interface
3. **Byte Order Matters**: Little-endian vs big-endian (x86 is little-endian)
4. **Alignment Matters**: CPU efficiency, hardware requirements
5. **Size Matters**: Every byte counts in bootloader (512 byte limit)

**Operating System Concepts:**

1. **Bootstrapping**: How to go from nothing to running code
2. **Memory Management**: Segmentation, paging, protection
3. **Filesystem Design**: How data is organized on disk
4. **Hardware Abstraction**: BIOS as lowest-level abstraction
5. **Mode Transitions**: Real mode → Protected mode → Long mode
6. **Interrupt Handling**: How hardware communicates with software

**Software Engineering:**

1. **Debugging Without Debugger**: Sometimes only have print statements
2. **Documentation is Critical**: Future you will thank past you
3. **Test Early, Test Often**: Bugs are exponentially harder in OS code
4. **Build System Design**: Automation saves time and prevents errors
5. **Version Control**: Git is essential for experimental changes

### Common Pitfalls and How We Avoided Them

**Pitfall 1: "Just Use a Bootloader Framework"**

Many tutorials suggest using GRUB or existing bootloaders.

**Why We Didn't:**
- Hides fundamental concepts
- Limits understanding of boot process
- Removes opportunity to learn filesystem implementation

**Result:** Deep understanding of boot process, FAT12, and bootloader design.

**Pitfall 2: "Skip Real Mode, Start in Protected Mode"**

Some modern tutorials jump straight to 32-bit.

**Why We Didn't:**
- BIOS requires real mode
- Real mode limitations teach why protected mode exists
- Understanding segmentation is foundational

**Result:** Appreciation for modern CPU features and why they evolved.

**Pitfall 3: "Use High-Level Language for Everything"**

Some projects try to write bootloader entirely in C/Rust.

**Why We Didn't:**
- Assembly is unavoidable for: BIOS calls, mode switches, segment setup
- Understanding assembly teaches how high-level languages work
- Some operations require specific instruction sequences

**Result:** Comfort with mixing C and assembly, understanding their trade-offs.

**Pitfall 4: "Ignore Build System, Just Use Shell Script"**

Simple projects often use ad-hoc build scripts.

**Why We Didn't:**
- Make provides dependency tracking (only rebuild what changed)
- Make enables parallel builds (faster development)
- Make is standard in OS dev community (easier for others to contribute)

**Result:** Fast, reliable builds that scale as project grows.

**Pitfall 5: "Don't Document Until It Works"**

Many projects defer documentation.

**Why We Didn't:**
- Documentation helps clarify design decisions
- Comments remind you why code exists
- Future debugging is easier with context

**Result:** This comprehensive README that explains not just what but why.

### Key Insights From Building an OS

**1. Hardware is Weirder Than You Think**

- A20 line exists because of DOS compatibility bugs
- Segment registers because 8086 had 20-bit bus but 16-bit registers
- 0x7C00 as boot address is arbitrary IBM PC BIOS decision
- FAT12 12-bit packing is space optimization from floppy era

**Lesson:** Modern hardware carries decades of legacy baggage.

**2. Abstractions Have Costs**

- BIOS is slow (mode switches on modern CPUs)
- Protected mode requires GDT setup
- Virtual memory requires page tables and TLB management
- System calls are slower than function calls

**Lesson:** Every abstraction has performance cost. Choose wisely.

**3. Debugging is Fundamentally Different**

In normal programming:
- printf() always works
- Debugger can step through code
- Crashes give stack traces
- Memory is protected

In OS development:
- Must implement printf() yourself
- Debugger setup is complex
- Crashes hang the machine
- Bugs can corrupt all memory

**Lesson:** OS development requires extreme care and different debugging strategies.

**4. Standards Evolved for Reasons**

- Boot signature (0xAA55) detects valid boot sectors
- FAT redundancy (two copies) prevents data loss
- GDT null descriptor catches null pointer dereferences
- Protected mode rings provide security boundaries

**Lesson:** Standards seem arbitrary until you understand their history.

**5. Real Mode Limitations Drove Evolution**

- 1 MB limit → Extended/Expanded memory solutions → Protected mode
- No protection → Programs crashed OS → Protected mode rings
- No multitasking → TSR hacks → Preemptive multitasking
- No GUI → Text mode → VGA → VESA → Modern GPUs

**Lesson:** Every modern OS feature exists to solve a real historical problem.

### Future Enhancements and Their Challenges

**Immediate Next Steps:**

1. **Implement Paging (Virtual Memory)**
   - **Challenge**: Page table setup, TLB management
   - **Benefit**: Memory isolation, swap space, demand paging
   - **Lesson**: Virtual memory is cornerstone of modern OS

2. **Set Up IDT and Handle Interrupts**
   - **Challenge**: Timer setup, keyboard driver, IRQ routing
   - **Benefit**: Responsive system, hardware abstraction
   - **Lesson**: Interrupts enable efficient hardware interaction

3. **Basic Memory Allocator**
   - **Challenge**: Fragmentation, efficiency, bookkeeping
   - **Benefit**: Dynamic memory, kernel data structures
   - **Lesson**: Memory management is non-trivial problem

**Medium-Term Goals:**

4. **User Mode and System Calls**
   - **Challenge**: Ring transitions, parameter validation
   - **Benefit**: Security, stability (user bugs don't crash kernel)
   - **Lesson**: Protection is essential for robust OS

5. **Simple Scheduler and Multitasking**
   - **Challenge**: Context switching, priority, fairness
   - **Benefit**: Multiple programs running simultaneously
   - **Lesson**: Scheduling is complex optimization problem

**Long-Term Goals:**

6. **64-bit Long Mode**
   - **Challenge**: IA-32e paging, new calling conventions
   - **Benefit**: Large address space, modern instructions (SSE, AVX)
   - **Lesson**: CPU architecture constantly evolves

7. **Modern Drivers (AHCI, NVMe, USB)**
   - **Challenge**: Complex specs, DMA, interrupts
   - **Benefit**: Support for modern hardware
   - **Lesson**: Device drivers are majority of OS code

### Recommended Learning Path

**For someone starting OS development:**

1. **Start Here (What We Did)**:
   - Simple bootloader (understand boot process)
   - FAT12 implementation (understand filesystems)
   - Real mode → Protected mode (understand CPU modes)

2. **Then Move To**:
   - Protected mode with paging
   - Interrupt handling (IDT, PIC)
   - Basic memory allocator

3. **Then Tackle**:
   - User mode and system calls
   - Simple multitasking
   - Basic device drivers

4. **Advanced Topics**:
   - 64-bit long mode
   - Multiprocessor support (SMP)
   - Modern devices (AHCI, USB)
   - Networking stack

**Don't try to do everything at once!** Each topic builds on previous knowledge.

### Resources That Helped This Project

**Essential Documentation:**
- Intel® 64 and IA-32 Architectures Software Developer Manuals
- OSDev Wiki (wiki.osdev.org) - Community knowledge base
- FAT Filesystem Specification by Microsoft
- Open Watcom C/C++ User's Guide
- NASM Assembly Language Manual

**Inspiration From:**
- Writing a Simple Operating System from Scratch (Nick Blundell)
- Operating Systems: Design and Implementation (Tanenbaum & Woodhull)
- Modern Operating Systems (Tanenbaum)
- Linux 0.01 source code (Linus Torvalds)
- MINIX source code (Andrew Tanenbaum)

**Why This Matters:**

OS development is unique:
- Requires understanding hardware AND software
- Spans lowest-level (assembly) to highest-level (abstractions)
- Teaches fundamentals that apply to all programming
- Builds appreciation for modern OS features

This project demonstrates that OS development is accessible with:
- Patience (debugging is slow)
- Persistence (many things will break)
- Curiosity (why does this work this way?)
- Documentation (write down what you learn)

Happy OS development! 🚀

---

*Last updated: December 2025*  
*Project phase: Two-stage bootloader with C integration (✅ Complete)*  
*Enhanced documentation: Comprehensive theory, implementation, and alternatives*  
*README enhancements: +2000 lines of detailed explanations, diagrams, and design rationale*  
*Next milestone: Interrupt handling (IDT) and memory management*  
*Lines of code: ~900 (assembly + C + build system)*  
*Current binary size: Stage 1: 512 bytes | Stage 2: ~2KB | Kernel: ~2 KB*

## Changelog

### Version 1.0.2 (2025-12-01)
- **Added**: New "Implementation Deep-Dive: Code Walkthrough" section with granular, line-by-line code explanations
- **Added**: Detailed Stage 1 bootloader code analysis with FAT12 BPB structure breakdown
- **Added**: Complete FAT12 cluster chain following explanation with bit manipulation examples
- **Added**: Stage 2 C/Assembly integration patterns with Open Watcom calling conventions
- **Added**: 64-bit Long Mode transition walkthrough with page table setup details
- **Added**: PIC remapping implementation explanation with I/O port details
- **Added**: Keyboard driver scancode mapping and interrupt handler implementation
- **Added**: VGA text mode direct memory access patterns
- **Added**: Shell command parsing implementation details
- **Updated**: Table of Contents to include new deep-dive sections

### Version 1.0.1 (2025-12-01)
- **Fixed**: Removed duplicate sections in README (Learning Resources, License, Acknowledgments)
- **Fixed**: Removed corrupted content mixed into documentation footer
- **Fixed**: Removed hardcoded path in build.sh script
- **Updated**: README last updated date to December 2025
- **Improved**: Overall README structure and consistency

### Version 1.0.0 (2025-11-30)
- **Added**: Enhanced logging system with log levels (INFO, OK, ERROR, DEBUG)
- **Added**: Detailed boot process logging in Stage 2 bootloader
- **Added**: VGA text mode helper functions in kernel
- **Added**: Color-coded kernel output with system details
- **Updated**: Kernel with comprehensive system information display
- **Updated**: README with current project status and changelog