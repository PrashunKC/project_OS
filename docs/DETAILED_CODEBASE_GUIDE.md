# project_OS / NBOS: Detailed Codebase Guide

## 1. Purpose, evidence, and reading conventions

This is the detailed technical reference for the current `project_OS` tree. It
is intentionally more analytical than a normal README: it explains the
execution order, binary contracts, function relationships, data movement,
build decisions, fallback behavior, and the alternatives that were rejected.

The source is the authority for behavior. Line ranges in this document refer
to the current files and are deliberately grouped by responsibility rather
than copied as a second source listing. A line-by-line explanation of a
bootloader is meaningful only when the line is understood in the context of
the BIOS, FAT, linker, and kernel contracts around it; therefore each range
below explains both the local operation and its downstream effect.

Where Git history or a comment explicitly records a reason, this guide calls
it a recorded reason. Where the implementation shows a constraint but does
not record an author decision, the guide labels the explanation as an
engineering inference. It does not invent historical intent.

The project is an educational BIOS-oriented x86 operating system. It is not
yet a protected production OS: the kernel has no complete user-mode loader,
the SDK syscall ABI is mostly a declared interface, and several alternate
boot files are experiments or legacy paths rather than active default paths.

## 2. One-page architecture

### 2.1 Active floppy path

```text
BIOS
  -> build/stage1.bin at 0x7C00
  -> FAT12 root directory
  -> STAGE2.BIN at its linker load address 0x20000
  -> stage2 real-mode setup and C entry cstart_(bootDrive)
  -> BIOS INT 13h disk reads through disk.c/x86.asm
  -> FAT12 parser in fat.c finds KERNEL.BIN
  -> kernel bytes: disk -> 0x30000 -> 0x100000
  -> optional VESA mode and framebuffer metadata at 0x8000
  -> jump to 0x100000 + 32 = 0x100020
  -> entry.asm establishes 32-bit/64-bit transition state
  -> start64() in kernel/main.c
  -> framebuffer terminal, VGA color terminal, or serial rescue terminal
  -> IDT/PIC/keyboard initialization
  -> shell_run()
```

The `+32` is not arbitrary. The linked kernel begins with a 28-byte Multiboot
header and alignment padding; `_start` is linked at `0x100020`. The historical
`+16` handoff entered the middle of the header and caused a VirtualBox triple
fault at `0x10001d`. The link address and stage2 constant are checked by
`test.sh`.

### 2.2 GRUB path

```text
BIOS -> GRUB El Torito image -> grub.cfg
     -> multiboot /boot/kernel.elf
     -> GRUB supplies magic/registers and possibly framebuffer metadata
     -> entry.asm -> start64()
```

The GRUB image deliberately sets `gfxpayload=text`. Earlier forced VBE/gfxterm
configuration produced unsupported graphical-mode failures and malformed GRUB
graphics borders in the tested QEMU/VirtualBox combinations. The kernel still
understands standard Multiboot framebuffer fields and legacy VBE information
when a bootloader supplies them; it simply does not require GRUB to force an
unreliable mode.

### 2.3 Console priority

`start64()` selects one and only one primary console backend:

1. **Framebuffer graphics terminal**: validated dimensions, pitch, bpp,
   memory model, and address; draws a desktop-like terminal window with the
   built-in 8x16 font.
2. **VGA color text**: writes 16-bit cells to `0xB8000`, updates the hardware
   cursor, and preserves the traditional 80x25 diagnostic path.
3. **Monochrome serial rescue**: initializes COM1 at `0x3F8` and emits bytes
   without touching VGA memory.

This order is a deliberate universal fallback design. A graphics-first policy
meets the visual requirement; VGA is more widely available than VESA; serial
is the least visually capable but remains useful when there is no display
device at all.

## 3. Build graph and why each layer exists

### 3.1 Root `Makefile`

The root [Makefile](/home/debian/Documents/GITHUB/MY_REPO/project_OS/Makefile)
is the reproducible graph from source to boot media.

| Lines | Responsibility and reason |
| --- | --- |
| 1-12 | Select NASM, host tools, output directories, and phony targets. Variables are overridable so CI or a cross-toolchain can substitute tools without editing recipes. |
| 15-41 | Define `all` and `requirements`. `all` stays small and deterministic: floppy image plus the host FAT inspector. The requirements matrix prevents users from discovering missing QEMU/GRUB/VirtualBox tools only after a long build. |
| 44-50 | Include `toolchain.mk`, then delegate stage2 and kernel builds to their own Makefiles. Delegation avoids mixing 32-bit loader flags with 64-bit kernel flags. |
| 53-68 | Create a FAT12 floppy, write the stage1 boot sector, and copy stage2/kernel as files. FAT is chosen because stage1 can find variable-sized files without hard-coding sector counts. |
| 72-93 | Build a data ISO containing the known-good floppy image and SDK. The ISO is not pretending that an ISO9660 filesystem is a FAT disk; it boots the embedded floppy image with El Torito floppy emulation. |
| 97-122 | Copy the SDK and assemble `crt0.o`. Keeping SDK output under `build/` prevents generated artifacts from becoming source inputs. |
| 126-153 | Build a GRUB BIOS ISO around `kernel.elf`. `gfxpayload=text` is a compatibility choice, not a claim that the kernel cannot draw graphics. |
| 157-176 | Build `iso_noemul` from the complete floppy image. The previous VBR-as-no-emulation design stopped at “Load Disk!” because a FAT VBR was being asked to read an ISO9660 device. |
| 180-201 | Build a 32 MiB FAT16 hard-disk image with an MBR, partition entry at sector 2048, VBR, stage2, and kernel. This is a separate layout from the floppy path. |
| 205-239 | Assemble stage1, MBR, VBR, and the host FAT tool. These targets are intentionally independent so a broken experimental HDD path does not prevent floppy development. |
| 242-269 | `always` creates output directories; `clean` removes generated state. The clean recipe removes known generated files only; it does not delete source. |
| 272-337 | VirtualBox setup, KVM conflict handling, VM recreation, and QEMU targets. The custom floppy VM disables hardware virtualization because the tested real-mode-to-long-mode path was unstable under accelerated VirtualBox execution. |

### 3.2 Stage2 and kernel Makefiles

`src/bootloader/stage2/Makefile` compiles freestanding 32-bit C and NASM
objects and links them with `stage2/linker.ld`. It uses the cross compiler when
available because stage2 cannot rely on the host's default ABI or libraries.

`src/kernel/Makefile` compiles the active kernel with host `gcc`/`ld` using
`-m64`, `-mcmodel=kernel`, `-mno-red-zone`, and no SIMD. The kernel is linked
to physical `0x100000` and converted from ELF to a flat `kernel.bin`.
`entry.asm` is part of the active object set; alternate `idt64.c` and `isr64.c`
files are retained as experiments/alternate implementations and are not
silently mixed into the active build.

The separation is essential: a 32-bit stage2 object and a 64-bit kernel object
have different calling conventions, relocation models, register widths, and
link addresses. Combining them would produce an artifact that can compile
while being impossible for the boot handoff to execute.

## 4. Physical memory and binary protocols

| Address/range | Producer | Consumer | Meaning |
| --- | --- | --- | --- |
| `0x7C00` | BIOS | stage1/legacy boot sectors | 512-byte boot sector execution address |
| `0x8000` | stage2 `main.c` | kernel `graphics.c` | packed `FramebufferInfo` handoff |
| `0x80FF` | stage2 real-mode assembly | stage2 C | VESA-available flag |
| `0x8100` | stage2 real-mode assembly | stage2 C | raw VBE mode-information block |
| `0x20000` | stage2 linker | stage1 | stage2 load/execute base |
| `0x30000` | stage2 C | stage2 C | temporary kernel read buffer |
| `0x100000` | stage2 C | kernel entry | final flat kernel load base |
| `0x100020` | kernel linker | stage2 jump | actual `_start` after header/padding |
| `0xB8000` | kernel `main.c` | VGA hardware | 80x25 color text cells |
| `0x3F8` | kernel `main.c` | COM1 | serial rescue output |

The packed framebuffer structure is a cross-language ABI. Its field widths and
order must remain synchronized between `stage2/vesa.h`,
`stage2/main.c`, `kernel/graphics.h`, and `kernel/multiboot.c`. Adding natural
alignment or changing a `uint16_t` to a `uint32_t` changes the bytes consumed by
the other side.

## 5. Boot sectors and assembly modules

### 5.1 `src/bootloader/stage1/boot.asm`

This is the active floppy first stage, loaded at `0x7C00`.

| Range | Operation | Why it is this way |
| --- | --- | --- |
| Header and BPB | Jump over the BIOS Parameter Block and define FAT12 geometry | BIOS and FAT tools expect the BPB at fixed offsets; the short jump leaves room for metadata. |
| Startup | Clear/setup segment registers, stack, and direction flag | Real-mode code cannot assume segment registers or string direction after BIOS entry. |
| Disk read helpers | Convert logical sectors to CHS and invoke `INT 13h` with retries | The floppy BIOS path is CHS-oriented and failure-prone; retries are safer than one silent failure. |
| FAT12 helpers | Read FAT and root directory, compare the 11-byte padded name `STAGE2  BIN`, follow the cluster chain | Stage2 is a file, not a fixed sector range; the loader must tolerate allocation changes made by `mcopy`. |
| Transfer | Load stage2 to `0x20000` and far-jump there | The address matches `stage2/linker.ld`; a far transfer reloads CS in real mode. |
| Signature/padding | Fill to 510 bytes and emit `0xAA55` | BIOS boot recognition requires the signature at bytes 510-511. |

Alternatives were direct fixed-sector loading and a larger single-stage C
loader. Fixed sectors are simpler but break when the FAT allocator moves the
file. A single stage cannot comfortably hold diagnostics, FAT traversal, VESA
setup, and kernel loading in 512 bytes, so stage1 is intentionally narrow.

### 5.2 `src/bootloader/stage2/main.asm`

This file is the assembly/C bridge. Its entry label establishes a valid C
stack and calls `cstart_` with the BIOS boot drive. Its later routines perform
real-mode VESA probing/setup before the C orchestration consumes the result.
The assembly is kept separate because BIOS interrupts and mode transitions
require exact register/segment behavior that a freestanding C compiler cannot
express portably.

The assembly writes the VESA flag and mode block at the fixed addresses listed
above. It also contains the transition glue required before the kernel's
64-bit entry code takes over. The exact instruction sequence is more important
than the C-like intent: changing operand size, segment assumptions, or stack
layout changes the ABI with `main.c`.

### 5.3 Legacy `src/bootloader/boot.asm`

This is an older single-stage FAT12 implementation. It contains its own BPB,
BIOS reads, FAT traversal, status strings, and kernel transfer. It is useful
as a reference and historical fallback, but the root build selects
`stage1/boot.asm` plus stage2 because splitting the responsibilities makes the
active path maintainable and leaves room for VESA and diagnostics.

### 5.4 `mbr/mbr.asm` and `vbr/vbr.asm`

`mbr.asm` emits the first 446 bytes of an MBR-style disk boot path and leaves
partition-table/signature space for the root Makefile. `vbr.asm` is written at
the FAT partition's first sector in the HDD image. Both are alternate layouts;
neither is the default floppy path.

The HDD path exists to test partitioned-media logic. It is not substituted for
the floppy path because partition offsets, BIOS drive numbering, and FAT16
geometry make it a separate proof problem. The failed no-emulation ISO
experiment demonstrated why a VBR cannot be used directly as an ISO boot
filesystem: the VBR reads FAT sectors, while the ISO exposes ISO9660 sectors.

### 5.5 `src/bootloader/stage2/x86.asm`, `x86.c`, and `x86.h`

`x86.asm` provides the small assembly routines that C needs for BIOS interrupt
calls, register/segment-sensitive operations, and low-level transfers.
`x86.c` supplies the C-visible wrappers and `x86.h` declares their contracts.
The wrappers isolate inline/assembly details so disk, VESA, and console code
can express intent (`DISK_ReadSectors`, `vesa_set_mode`, `putc`) instead of
duplicating register setup.

## 6. Stage2 C modules, function traces, and alternatives

### 6.1 `disk.c` / `disk.h`

`DISK_Initialize` stores the BIOS drive number and asks BIOS for geometry.
`div32` performs a small freestanding division helper without assuming a
runtime library. `DISK_LBA2CHS` maps an LBA to cylinder/head/sector using the
drive's reported geometry. `DISK_ReadSectors` issues bounded BIOS reads and
retries failures.

The call chain is:

```text
cstart_
  -> DISK_Initialize
  -> FAT_Initialize
       -> FAT_readBootSector
       -> FAT_ReadFat
       -> FAT_ReadRootDirectory
             -> DISK_ReadSectors
  -> FAT_Open/FAT_Read
       -> DISK_ReadSectors
```

CHS plus BIOS is chosen for the floppy because it is available before a disk
driver or protected-mode environment exists. An LBA BIOS extension path would
be more capable for HDDs, but it would add packet structures, extension
checks, and another compatibility branch. The HDD/VBR path can evolve toward
that independently.

### 6.2 `fat.c` / `fat.h`

The packed `FAT_BootSector` and `DirectoryEntry` mirror on-disk bytes. The
packing directive is a correctness requirement, not an optimization.

Function flow:

- `FAT_Initialize` places `FAT_Data` at `MEMORY_FAT_ADDR`, reads the BPB,
  reserves space for the FAT and root directory, reads both, initializes the
  synthetic root-directory handle, and resets ten file slots.
- `FAT_ClusterToLba` maps cluster `n` to the data region using
  `data_lba + (n - 2) * sectors_per_cluster`; FAT numbers 0 and 1 are reserved.
- `FAT_NextCluster` decodes the packed 12-bit FAT12 entry: even clusters use
  the low 12 bits and odd clusters use the high 12 bits.
- `FAT_OpenEntry` finds a free handle, initializes position/cluster state, and
  preloads its first sector.
- `FAT_Read` copies from the current sector buffer, advances position, follows
  the next cluster when needed, and stops at file size or end-of-chain.
- `FAT_FindFile` converts a dotted path component to uppercase padded 8.3
  form, then scans directory entries with `FAT_ReadEntry`.
- `FAT_Open` walks slash-separated components, enforcing that intermediate
  components are directories.
- `FAT_Close` resets root state or releases a normal file slot.

The loader uses an in-memory FAT/root cache rather than reading a sector for
every directory comparison. The tradeoff is fixed low-memory consumption and
limited file handles in exchange for predictable boot-time I/O. A full VFS,
long-file-name support, write support, or dynamically allocated handles would
be excessive before the kernel exists.

### 6.3 `vesa.c` / `vesa.h`

`vesa_get_controller_info` and `vesa_get_mode_info` query BIOS VBE services.
`vesa_set_mode` selects a mode and `vesa_init` validates the controller,
enumerates/chooses a usable 32-bit linear framebuffer, and records the mode
metadata. The fields include address, width, height, pitch, bits per pixel,
memory model, and color-field positions.

The loader uses VBE before protected mode because BIOS is the only graphics
service available at that point. A fixed VGA mode would be simpler but cannot
provide the requested graphical terminal. A full native GPU driver would be
more reliable on modern hardware but is far beyond a BIOS educational kernel.

The mode information is copied into the compact shared `FramebufferInfo`
structure by `stage2/main.c`. When VESA is unavailable, the structure is
zeroed deliberately: zero is an explicit “no framebuffer” signal and lets the
kernel select VGA without trusting stale low memory.

### 6.4 `stdio.c` / `stdio.h`

`putc` emits one diagnostic character through the stage2 output primitive;
`puts` emits a string; the private unsigned/signed formatting helpers support
the limited `%x`, `%u`, `%d`, `%s`, and related formats used by boot messages;
`printf` parses the format without libc.

A freestanding formatter is chosen because stage2 cannot link the host C
library. It is intentionally smaller than `printf` from libc: unsupported
format features would increase boot image size without helping diagnostics.

### 6.5 `memory.c`, `string.c`, `ctype.c`, `utility.c`

These are minimal runtime replacements:

- `memset` fills a bounded byte range; `memcmp` compares it.
- `memcpy`/`strlen` provide byte copying and string length needed by FAT and
  orchestration.
- `isLower` and `toupper` normalize FAT names.
- `align` rounds a byte count to a sector boundary.

The alternative is linking compiler runtime/libc objects. That would make
stage2 dependent on a host ABI and potentially pull in unavailable system
calls, so local helpers are safer and auditable.

### 6.6 `stage2/main.c`

`cstart_(bootDrive)` is the high-level state machine. Its seven printed steps
are also its diagnostic checkpoints:

1. Initialize BIOS disk and print geometry.
2. Parse FAT metadata and cache the filesystem.
3. Open `/kernel.bin`.
4. Read the complete file into `0x30000`.
5. Copy it to `0x100000`.
6. Convert VESA mode bytes at `0x8100` into `FramebufferInfo` at `0x8000`.
7. Build a minimal Multiboot info array and jump to `0x100020`.

The temporary buffer prevents disk reads from overwriting the final execution
region while the file is still being traversed. The explicit `read != size`
check prevents a truncated kernel from being executed. The static zeroed
Multiboot array is a compatibility bridge for the kernel entry contract; GRUB
provides a richer structure on the GRUB path.

The `goto end` branches are intentional boot-failure terminals. A bootloader
cannot recover by returning to an absent caller; it prints a reason and
halts, which is more diagnosable than executing undefined memory.

## 7. Kernel entry and initialization

### 7.1 `entry.asm`, `linker.ld`, and `long_mode_init.asm`

`entry.asm` begins with the Multiboot header, aligns `_start`, establishes the
initial page/stack state, and performs the 32-bit-to-64-bit transition before
calling `start64`. `linker.ld` places `.multiboot` first and links the image at
1 MiB. `long_mode_init.asm` contains reusable control-register/GDT/paging
transition logic for the long-mode environment.

The header-first layout is required by GRUB's Multiboot scanner. The alignment
makes entry predictable for the stage2 jump and for page-table assumptions.
The flat `kernel.bin` preserves the same byte layout after `objcopy`; the ELF
is retained for GRUB symbols and validation.

### 7.2 `main.c`: console and boot lifecycle

`start64(magic, mbi_addr)` initializes Multiboot/framebuffer state, chooses the
console, displays the welcome/terminal UI, initializes IDT and interrupt
dispatch, remaps the PIC, enables interrupts, initializes the keyboard, and
enters `shell_run`.

The local helpers have distinct contracts:

- `console_width`/`console_height` abstract framebuffer-window dimensions from
  VGA dimensions.
- `serial_init` and `serial_putc` implement the final rescue output.
- `graphics_terminal_init` computes a centered terminal window, title bar,
  clipping area, and text cursor geometry.
- `update_cursor`, `set_cursor_row`, and `set_cursor_col` update VGA hardware
  only; framebuffer and serial backends have no VGA cursor.
- `scroll_screen` copies framebuffer pixels inside the terminal window or
  shifts VGA cells; serial scrolling is naturally delegated to the terminal.
- `clear_screen`, `putchar_at`, `kputc`, `kprint`, `knewline`, and
  `kprint_line` are the backend-neutral output API.
- `restore_vga_text_mode` provides a safe VGA fallback/reset operation.

The backend is an enum rather than a collection of booleans. That prevents
contradictory states such as “framebuffer and serial both own the cursor.”
The color byte is retained in the common API so shell code does not need
backend-specific branches; monochrome simply ignores it.

### 7.3 `graphics.c` / `graphics.h`

`graphics_init` validates the framebuffer handoff before enabling drawing.
`graphics_is_available` is the gate used by console selection. The primitive
chain is:

```text
graphics_put_pixel / graphics_get_pixel
  -> pitch-aware byte address
graphics_clear
  -> fill entire active framebuffer
graphics_draw_rect / graphics_fill_rect
graphics_draw_line
  -> integer incremental line algorithm
graphics_draw_circle / graphics_fill_circle
  -> midpoint-style rasterization/fill
graphics_draw_char
  -> built-in 8x16 bitmap glyph
graphics_draw_string
  -> repeated glyphs with x advance
```

Bounds checks are important because framebuffer addresses come from firmware.
Pitch is used instead of assuming `width * bytes_per_pixel`; VBE modes often
pad each scanline. The renderer is intentionally software-based: it works
with any linear framebuffer and needs no GPU driver. A font library or GPU
acceleration would provide more features but would add storage, initialization,
and hardware assumptions.

### 7.4 `multiboot.c` / `multiboot.h`

`multiboot_init` validates the magic/flags and reads the standard framebuffer
fields. It also checks the legacy VBE mode-information pointer used by some
GRUB BIOS implementations. The accessors return address, dimensions, pitch,
and bpp to graphics initialization.

Supporting both formats is a compatibility choice based on observed GRUB
behavior: some loaders expose a usable mode block without consistently
setting the newer framebuffer flag. Rejecting legacy data would make the
graphics terminal disappear on otherwise valid boots; trusting every pointer
without validation would risk drawing to arbitrary memory.

### 7.5 `idt.c`, `idt.h`, `idt64.c`, `idt64.h`, `idt64_load.asm`

The active `idt.c`/`idt.h` path defines the descriptor representation and
`idt_set_gate`/`idt_init`. The `idt64.*` files provide an alternate explicit
64-bit implementation and `idt64_load.asm` supplies an assembly `lidt` helper.
The retained variants document the transition from the initial interrupt
implementation; the Makefile selects one coherent path rather than linking
both.

An IDT is required before enabling interrupts. The descriptor stores the
handler address split into architecture-defined fields, selector, gate type,
and presence bit. A high-level C table is easier to maintain than embedding
handlers at every call site; the assembly stubs still own the exact CPU frame.

### 7.6 `interrupts.asm`, `interrupts64.asm`, and ISR C modules

Assembly stubs save registers, normalize error-code/no-error-code exceptions,
and call C dispatch. `isr.c` handles the active 32-bit-shaped register frame;
`isr64.c` is the alternate long-mode frame implementation. `isr_init` installs
exception handlers, `register_interrupt_handler` fills the callback table,
`isr_handler` reports CPU exceptions, and `irq_handler` dispatches hardware
IRQs.

The frame layout is a binary protocol: the C struct must match the push order
in assembly. This is why the assembly is not replaced by ordinary C
functions—C cannot control the CPU's automatic exception frame and exact
register save order portably.

Page faults use `CR2` to report the faulting address. Unknown exceptions print
the vector and register state, then halt or return according to the handler
policy. Reporting state is preferred to silently looping because it makes
emulator and VirtualBox failures diagnosable.

### 7.7 `i8259.c` / `i8259.h`

`i8259_init` sends the PIC initialization command sequence, remaps IRQs away
from CPU exception vectors, and restores masks. `i8259_send_eoi` acknowledges
the master/slave PIC as needed. `i8259_disable` masks all lines.

The legacy PIC is chosen because PS/2 keyboard input is available on simple
BIOS/emulator hardware and is much smaller than an APIC implementation.
APIC support is a future alternative for SMP and modern interrupt routing,
not a requirement for this single-CPU educational target.

### 7.8 `keyboard.c` / `keyboard.h`

The IRQ1 handler reads set-1 scancodes from port `0x60`, tracks shift/caps
state, converts make codes to characters, and pushes characters into a
bounded 32-byte queue. `keyboard_has_key`, `keyboard_get_key`, and
`keyboard_get_key_nonblocking` expose blocking and nonblocking consumers.
`keyboard_init` resets the queue and controller state.

The queue decouples interrupt timing from shell processing: the ISR does not
draw or parse commands, so a slow shell cannot make the interrupt handler
perform unbounded work. Overflow is bounded rather than dynamically allocated
because the kernel has no heap requirement at this stage. Stale state is
reset on warm boots to prevent the welcome screen or first command from
being contaminated by old scancodes.

### 7.9 `io.c`, `io.h`, `vga.c`, and `vga.h`

`io.c` provides the minimal `outb` port write used by hardware modules.
`vga.c` detects the VGA text adapter and sets the hardware cursor through
ports `0x3D4/0x3D5`. `main.c` owns the actual text cell writes because it
also owns console selection and scrolling.

This split keeps port I/O reusable while keeping policy (which backend is
active) in one place. A full terminal driver object would be cleaner for a
larger OS, but the current enum plus shared functions is easier to audit while
the ABI is changing.

## 8. Shell module

`src/kernel/shell.c` is both the interactive command parser and a diagnostic
client of the console/keyboard APIs.

| Functions | Behavior |
| --- | --- |
| `strlen`, `strcmp`, `strncmp` | Freestanding string helpers; `strncmp` is currently unused and is reported as a compiler warning. |
| `show_prompt` | Prints the prompt using the kernel output API. |
| `cmd_help` | Lists available commands. |
| `cmd_clear` | Clears the selected console backend. |
| `cmd_echo` | Emits the command arguments. |
| `cmd_reboot` | Requests keyboard-controller/CPU reset behavior. |
| `cmd_shutdown` | Requests emulator/ACPI-style shutdown where supported. |
| `cmd_halt` | Disables or stops execution. |
| `cmd_peek` | Parses a hexadecimal address and prints 16 bytes; intentionally unsafe for arbitrary addresses, so it is a debugging command. |
| `cmd_about` | Prints project identity/version information. |
| `cmd_info` | Prints backend, cursor, and system diagnostics. |
| `execute_command` | Tokenizes the current line and selects a built-in. |
| `redraw_line` | Repaints the editable line after insertion/deletion/arrow movement. |
| `shell_putchar` | Adds a character to the line buffer or handles editing. |
| `shell_handle_arrow` | Handles cursor movement and history-line editing. |
| `shell_init` | Clears line state and installs initial prompt state. |
| `shell_run` | Polls/consumes keyboard input and dispatches editing/commands forever. |

The shell uses kernel output functions instead of writing `0xB8000` directly.
That is what makes the same shell operate on the graphical, VGA, and serial
backends. A richer command parser or heap-backed history is possible later,
but fixed buffers keep early-boot behavior deterministic.

## 9. SDK and application-facing modules

### 9.1 `sdk/include/stdint.h`

Defines fixed-width integer aliases and limits without importing the host
libc. Application and kernel code must agree on widths for syscall arguments.

### 9.2 `sdk/include/nbos.h`

The syscall constants and inline wrappers define the intended application ABI.
`syscall0` through `syscall4` place the number in `RAX`, arguments in the
documented registers, and execute `int $0x80`. Convenience functions
`print`, `getkey`, `kbhit`, `free`, `exit`, `sleep`, `strlen`, and `strcmp`
wrap those primitives or provide local utility behavior.

The ABI is declared before the complete user-mode kernel exists so applications
can be developed against a stable contract. The alternative would be direct
kernel calls, which would couple applications to kernel addresses and make
future privilege separation impossible. The important current limitation is
that not every declared syscall has matching kernel dispatch yet.

### 9.3 `sdk/include/graphics.h`

This is a header-only BGI-style compatibility layer. `initgraph` queries screen
dimensions, state setters update current foreground/background/fill values,
and pixel/line/rectangle/bar/circle/ellipse/text helpers issue graphics
syscalls. `_bgi_to_rgb` maps the small BGI palette into packed RGB.

The API is inline to keep small examples self-contained and avoid a separate
SDK runtime library. `arc` and viewport clipping are intentionally simplified
compatibility shims; they preserve source-level API shape without claiming
full BGI fidelity.

### 9.4 `sdk/lib/crt0.asm`, wrappers, and examples

`crt0.asm` is the application entry object: it provides the startup symbol,
calls `main`, and exits through the ABI. `sdk/tools/nbos-gcc` selects a
cross-compiler when present and falls back to host tools with freestanding
64-bit flags. `sdk/tools/nbos.mk` centralizes include paths and linker flags.

`examples/hello/hello.c` proves console output; `graphics/demo.c` exercises
the drawing API; `game/pong.c` combines keyboard and graphics into a small
interactive workload. Each example has an independent Makefile so SDK
experimentation does not alter the OS boot image.

## 10. Host FAT tool

`tools/fat/fat.c` is deliberately not the bootloader FAT library. It uses
host `FILE*`, `malloc`, and standard I/O to inspect an image after it is built.

`readBootSector` parses BPB fields, `readSectors` seeks to LBAs, `readFat`
loads the first FAT, `readRootDirectory` loads directory entries,
`findFile` compares the raw 11-byte FAT name, and `readFile` follows the
cluster chain into an output buffer. `main` validates arguments, opens the
image, runs those stages, and prints readable bytes with non-printable values
escaped.

Keeping this tool separate avoids accidentally making the bootloader depend on
host libc and provides a useful proof tool for the image construction path.

## 11. Build scripts and diagnostics

### 11.1 `build_scripts/toolchain.mk`

Discovers `$HOME/opt/cross/bin/i686-elf-*` and falls back to host tools. The
fallback is practical for environments where the cross compiler is absent;
stage2 still requires compatible 32-bit output, so the diagnostic scripts
must report which compiler was actually selected.

### 11.2 `check_toolchain.sh` and `setup_toolchain.sh`

`check_toolchain.sh` performs non-destructive command and version probes.
`setup_toolchain.sh` checks prerequisites, downloads/configures binutils and
GCC, and may install host packages or VirtualBox. It is intentionally
separate from normal builds because installation and elevated privileges are
not appropriate in every environment.

### 11.3 `test.sh`

The diagnostic harness has three evidence classes:

1. **Raw evidence** in `test.log`, QEMU logs, VirtualBox logs, and screenshots.
2. **Warnings** in `warnings.txt` for suspicious signatures that do not prove
   failure, such as a reset line or an unused-function compiler warning.
3. **Errors** in `errors.txt` for failed proof steps.

Its proof sequence is:

```text
capture host/tool/virtualization state
  -> build stage1, stage2, kernel, floppy, SDK, optional images
  -> validate artifacts, recipes, linked entry offset
  -> observe QEMU with monitor screenshots and bounded timeout
  -> optional VirtualBox host report
  -> optional VirtualBox run-vbox boot, screenshot, VM log, fault scan
  -> classify diagnostics and write summary
```

QEMU timeout is expected because the kernel intentionally keeps running; an
early non-zero exit or known loader signature is a failure. The VirtualBox
proof is stronger than “the command returned”: it waits for `running`,
captures the display, scans fresh boot/VM logs, and powers off the VM. Normal
configuration text such as `Triple Fault Reset: disabled` and
`VERR_HGCM_SERVICE_NOT_FOUND` is excluded from guest-fault matching because
those lines describe VirtualBox configuration/services, not a guest crash.

### 11.4 `run.sh` and `build.sh`

`run.sh` is the short interactive convenience path: build the floppy and start
QEMU. `build.sh` performs a clean-style build and saves command output. They
are intentionally less strict than `test.sh`; use the latter when evidence,
logs, and proof classification matter.

## 12. Complete data and instruction flows

### 12.1 Kernel file flow

`kernel.c/asm` -> `kernel.elf` -> `objcopy` -> `kernel.bin` -> `mcopy` into
FAT12 -> stage1 reads FAT -> stage2 reads sectors -> `0x30000` -> `memcpy` ->
`0x100000` -> `_start` at `0x100020`.

The critical invariants are:

- the FAT directory name is `KERNEL  BIN` after uppercase 8.3 conversion;
- the file size is read from the directory entry, not assumed;
- the temporary buffer is not the execution address;
- the linker address and stage2 jump offset agree;
- stage2 does not jump until the complete file has been read.

### 12.2 Framebuffer metadata flow

VBE BIOS call -> raw mode block at `0x8100` -> stage2 field extraction ->
packed `FramebufferInfo` at `0x8000` -> `multiboot_init`/graphics validation
-> `graphics_init` -> pixel address `base + y*pitch + x*bytes_per_pixel`.

GRUB can bypass the stage2 copy and provide Multiboot/VBE metadata. The
kernel accepts both routes but validates before drawing. Zeroed metadata
selects fallback rather than causing an invalid memory write.

### 12.3 Keyboard instruction flow

Keyboard hardware -> IRQ1 -> PIC vector -> assembly ISR stub -> C
`irq_handler` -> registered keyboard handler -> scancode translation -> ring
buffer -> `shell_run` -> line editing -> command handler -> console output.

The interrupt path is short and bounded. Shell parsing stays outside the ISR,
which prevents long drawing or command operations from blocking hardware
acknowledgement.

### 12.4 Console output flow

`kprint` -> backend enum:

- framebuffer: glyph bitmap -> `graphics_put_pixel`;
- VGA: cell index -> `0xB8000` character/attribute pair;
- serial: wait for COM1 transmit-empty -> port `0x3F8`.

This shared flow is why the graphical terminal, color VGA fallback, and
monochrome rescue output remain behaviorally consistent.

### 12.5 Application syscall flow

SDK function -> register ABI (`RAX` plus arguments) -> `int 0x80` -> intended
kernel syscall dispatcher -> console/input/memory/graphics service. The first
two stages exist in the SDK today; the complete dispatcher and user-mode
isolation are future work. This distinction prevents examples from being
mistaken for proof that user applications already boot and run.

## 13. Failure history and the design decisions it explains

### Linker/entry failure

The Multiboot header grew/aligned the entry so `_start` became `0x100020`.
Stage2 still jumped to `+16`, entering header bytes. QEMU could appear alive
long enough to mislead a smoke test, while VirtualBox reported a guest triple
fault. The fix is the shared constant/check and the `nm`-based proof in
`test.sh`.

### QEMU `-kernel` failure

QEMU's `-kernel` option is a Linux-style loader, not a generic Multiboot flat
binary loader. Passing `kernel.bin` directly produced “Error while loading
elf kernel”/VBE messages. `qemu-test` now boots the FAT floppy so the same
stage1/stage2 path is exercised.

### `iso_noemul` “Load Disk!”

The old target used a FAT VBR as an ISO no-emulation boot image. BIOS handed
the VBR an ISO device, but the VBR expected FAT sectors and BIOS disk
semantics. The target now embeds the complete known-good floppy image with
standard floppy emulation.

### GRUB graphics errors

Forcing `insmod vbe`, `gfxterm`, and `640x480x32` made GRUB fail on firmware/
emulator combinations that did not support that mode. Text payload removes
the fragile GRUB dependency while retaining kernel-side framebuffer parsing
when metadata is genuinely available.

### VirtualBox/KVM conflict and stale VMs

VirtualBox and KVM compete for AMD-V/VT-x. `check-vbox-host` reports and tries
to release inactive KVM modules; it refuses to claim success if KVM remains
owned. VM targets stop/unregister old named VMs before creation to avoid
“settings file already exists.” These operations are logged by the diagnostic
script rather than hidden.

### Test harness false confidence

The original test only waited for QEMU to remain alive. The current harness
still treats a running kernel as expected, but adds artifact proofs, recipe
checks, entry-offset checks, known error scans, screenshots, and optional
VirtualBox boot evidence. A warning is visible and preserved; it is not
silently converted into a pass or failure.

## 14. Alternatives and current limitations

| Area | Current choice | Alternative | Why current choice |
| --- | --- | --- | --- |
| Firmware | BIOS/real mode | UEFI | Matches existing boot sectors and keeps the learning path observable. |
| First-stage FS | FAT12 | Fixed sectors/ext2 | FAT is mutable and inspectable; fixed sectors are brittle; ext2 is too large here. |
| Kernel handoff | Flat binary + explicit jump | ELF loader | Small and deterministic for stage2; GRUB separately uses ELF. |
| Graphics | VBE linear framebuffer | VGA-only or GPU driver | Meets graphical-terminal goal without a vendor driver. |
| Interrupts | 8259 PIC | APIC/IOAPIC | Simpler single-CPU emulator path. |
| Input | PS/2 set-1 | USB HID | Available in BIOS/QEMU/VirtualBox without USB stack. |
| Console | Software renderer | Font/GPU stack | Portable over any linear framebuffer. |
| Memory | fixed low-memory buffers | heap/VFS | Predictable before kernel memory management. |
| Test boot | QEMU plus optional VirtualBox | hardware CI | Reproducible local evidence without requiring physical hardware. |
| SDK ABI | `int 0x80` declarations | direct calls | Creates a stable future user/kernel boundary. |

Current limitations that should not be hidden:

- GRUB framebuffer support is metadata-dependent; GRUB is intentionally text
  payload in the current target.
- The final on-screen monochrome fallback is serial, not a white VGA screen.
- `paging.c` reads control registers and exposes an initializer interface, but
  page tables are prepared by assembly and full dynamic memory management is
  not implemented.
- SDK syscalls are declared more broadly than the kernel currently dispatches.
- `shell.c` contains an unused `strncmp`, which is a known compiler warning.
- The HDD, legacy boot, alternate IDT/ISR, and SDK paths need separate proof
  before being called production-ready.

## 15. Module inventory and line-coverage index

The following inventory makes the “line by line” scope explicit. For each file,
read the listed ranges in order; the preceding sections explain the contract
and rationale that gives each range meaning.

### Root and host files

| File | Covered responsibilities |
| --- | --- |
| `Makefile` | Variables/targets 1-50; image recipes 53-201; boot/tool/clean 205-269; VirtualBox/QEMU/test 272-340 |
| `build.sh` | Clean/build command sequencing and output capture |
| `run.sh` | Interactive floppy build and QEMU launch |
| `test.sh` | Argument parsing; host capture; build/proof steps; QEMU monitor; VirtualBox evidence; warning/error classification |
| `watcom_64_32_dividing_implimentation.cpp` | Standalone division experiment; not in the active graph |
| `test.txt` | Small boot/test input artifact; not linked into the default image |

### Bootloader files

| File | Covered responsibilities |
| --- | --- |
| `src/bootloader/stage1/boot.asm` | BPB/startup; BIOS CHS reads; FAT12 lookup; stage2 load; signature |
| `src/bootloader/stage2/main.asm` | C entry, stack/segments, BIOS/VESA setup, transition glue |
| `src/bootloader/stage2/main.c` | `cstart_` seven-step orchestration and kernel handoff |
| `src/bootloader/stage2/disk.c/.h` | geometry, LBA/CHS, retrying reads |
| `src/bootloader/stage2/fat.c/.h` | packed FAT structures, cache, handles, cluster/file traversal |
| `src/bootloader/stage2/vesa.c/.h` | VBE controller/mode queries and framebuffer ABI |
| `src/bootloader/stage2/stdio.c/.h` | freestanding diagnostics |
| `src/bootloader/stage2/memory.c/.h` | byte operations |
| `src/bootloader/stage2/string.c/.h` | string operations |
| `src/bootloader/stage2/ctype.c/.h` | FAT-name normalization |
| `src/bootloader/stage2/utility.c/.h` | alignment helper |
| `src/bootloader/stage2/x86.asm/.c/.h` | assembly hardware wrappers |
| `src/bootloader/stage2/memdefs.h` | fixed low-memory allocation contract |
| `src/bootloader/stage2/linker.ld/.lnk` | stage2 physical layout/linker alternatives |
| `src/bootloader/boot.asm` | legacy single-stage FAT path |
| `src/bootloader/mbr/mbr.asm` | MBR experiment |
| `src/bootloader/vbr/vbr.asm` | partition VBR/HDD experiment |

### Kernel files

| File | Covered responsibilities |
| --- | --- |
| `src/kernel/entry.asm` | Multiboot header, aligned entry, early CPU transition |
| `src/kernel/linker.ld` | 1 MiB layout and section order |
| `src/kernel/long_mode_init.asm` | long-mode support routines |
| `src/kernel/main.c` | boot lifecycle, console abstraction, fallback selection |
| `src/kernel/graphics.c/.h` | framebuffer validation, raster primitives, font |
| `src/kernel/multiboot.c/.h` | standard/legacy framebuffer metadata |
| `src/kernel/idt.c/.h`, `idt64.c/.h`, `idt64_load.asm` | IDT variants/load operation |
| `src/kernel/interrupts.asm`, `interrupts64.asm` | CPU/IRQ entry stubs |
| `src/kernel/isr.c/.h`, `isr64.c/.h` | exception/IRQ dispatch variants |
| `src/kernel/i8259.c/.h` | PIC initialization and EOI |
| `src/kernel/keyboard.c/.h` | PS/2 translation and queue |
| `src/kernel/io.c/.h` | port output |
| `src/kernel/vga.c/.h` | VGA detection/cursor |
| `src/kernel/paging.c/.h` | control-register access and paging interface |
| `src/kernel/shell.c/.h` | commands, editing, command loop |
| `src/kernel/stdint.h` | freestanding integer types |
| `src/kernel/Makefile` | active 64-bit object/link configuration |

### SDK, tools, and build-support files

| File/group | Covered responsibilities |
| --- | --- |
| `sdk/include/nbos.h` | syscall ABI and convenience wrappers |
| `sdk/include/graphics.h` | BGI-compatible inline graphics |
| `sdk/include/stdint.h` | application integer types |
| `sdk/lib/crt0.asm` | application startup |
| `sdk/tools/nbos-gcc`, `nbos.mk`, `env.sh` | compiler/path setup |
| `sdk/examples/*` | hello, graphics, and Pong consumers |
| `tools/fat/fat.c` | host image inspection |
| `build_scripts/*.sh` | checks and toolchain setup |
| `build_scripts/*.mk` | compiler discovery |
| `build_scripts/*.md` | command/reference documentation |

## 16. How to extend the project safely

1. Update the binary contract first: linker address, packed structure, or
   register frame changes must be reflected on both sides.
2. Keep boot stages freestanding and bounded; do not introduce libc calls.
3. Add a proof to `test.sh` for every new boot invariant.
4. Preserve raw logs; classify suspicious evidence instead of suppressing it.
5. Test the exact path changed: floppy, ISO, GRUB, HDD, or VirtualBox.
6. Update this guide and the component README when a module's ownership or
   fallback order changes.
7. Do not call a target “universal” unless its firmware, emulator, and
   metadata assumptions are documented and tested.

