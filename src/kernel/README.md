# Kernel

The kernel is a freestanding x86-64 long-mode image. Its assembly entry path
sets up the initial execution state and calls `start64` in `main.c`.

## Initialization flow

`start64` initializes the framebuffer and Multiboot metadata, creates and
loads the interrupt descriptor table, registers exception/IRQ stubs, remaps
the 8259 PIC, enables interrupts, and registers the PS/2 keyboard handler.
The graphical welcome screen is shown when stage2 supplies a valid framebuffer.
The console backend is selected in this order:

1. A validated framebuffer uses the graphical terminal with the built-in 8x16
   renderer. The welcome screen transitions into a persistent desktop-style
   terminal window; shell scrolling is clipped to that window.
2. If no framebuffer is available, a detected VGA adapter uses the color
   80x25 text terminal.
3. If VGA is unavailable, the kernel uses a monochrome serial terminal on
   COM1 (115200/8N1) without touching VGA memory.

After a keypress, the shell continues on the selected backend. Color
attributes are ignored by the monochrome fallback.

The Multiboot entry also requests a 640x480x32 VBE mode and accepts the
legacy Multiboot VBE mode-information block when GRUB provides one. This
keeps the framebuffer path usable across GRUB BIOS implementations that do
not set the newer framebuffer flag consistently.
Keyboard and shell state are explicitly reset on warm boots so stale
scancodes cannot skip the welcome screen or contaminate the first command.

## Modules

| Module | Responsibility |
| --- | --- |
| `entry.asm`, `long_mode_init.asm` | Entry and long-mode setup |
| `interrupts.asm`, `idt64_load.asm` | Assembly interrupt entry and IDT loading |
| `idt.*`, `idt64.*` | IDT descriptor formats and gate initialization |
| `isr.*`, `i8259.*` | Exception/IRQ dispatch and PIC remapping |
| `keyboard.*` | Set-1 PS/2 scancode translation and 32-byte input queue |
| `graphics.*` | Framebuffer pixels, primitives, and 8x16 ASCII rendering |
| `multiboot.*` | Optional GRUB framebuffer metadata parsing |
| `paging.*` | Control-register access; page tables are prepared in assembly |
| `vga.*`, `io.*` | VGA cursor I/O and port output |
| `shell.*` | Line editing and built-in diagnostic commands |

The public headers describe the binary layouts and contracts shared across
these modules. In particular, interrupt handlers receive the register frame
created by `interrupts.asm`, and graphics consumes the packed `FramebufferInfo`
structure written by the bootloader.

## Shell commands

The built-in shell currently supports `help`, `clear`, `echo`, `peek`,
`reboot`, `shutdown`, `halt`, `about`, and `info`. `peek` reads 16 bytes from
the supplied address, so it should only be used with known-safe addresses.

## Building

```bash
make -C src/kernel
```

The kernel Makefile emits `kernel.elf` and a flat `kernel.bin`. It excludes
the alternate `idt64.c` and `isr64.c` implementation files from the default
kernel object list because the active build uses the corresponding 32-bit
named source files with 64-bit compilation flags.
