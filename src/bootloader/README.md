# Bootloader

The bootloader is split into BIOS-compatible stages so the 512-byte boot
sector stays small while filesystem and hardware logic can live in C.

## Stages

- `mbr/mbr.asm` builds an MBR-style boot sector for the hard-disk image.
- `vbr/vbr.asm` builds the partition volume boot record and supports the
  no-emulation/hard-disk layouts.
- `stage1/boot.asm` is the floppy path's first stage. It runs in 16-bit real
  mode, reads the FAT12 metadata, follows the `STAGE2.BIN` cluster chain, and
  transfers control to stage2.
- `stage2/main.asm` provides the entry/real-mode setup glue. `main.c` performs
  the high-level boot sequence.

## Stage2 subsystems

| Module | Responsibility |
| --- | --- |
| `disk.*` | BIOS disk geometry and LBA-to-CHS reads |
| `fat.*` | FAT boot-sector, FAT table, root-directory, and file reads |
| `x86.*` | BIOS interrupt and teletype wrappers |
| `vesa.*` | VBE discovery, mode selection, and framebuffer metadata |
| `memory.*`, `string.*`, `ctype.*`, `utility.*` | Small freestanding helpers |
| `stdio.*` | Serial-free text output through BIOS teletype calls |
| `memdefs.h` | Fixed low-memory regions shared by stage2 routines |

Stage2 reads `kernel.bin` into a temporary buffer at `0x30000`, copies it to
`0x100000`, writes framebuffer metadata at `0x8000`, and jumps to the aligned
kernel entry at `0x100020` (the 32-byte Multiboot header area precedes it).
The kernel must continue to agree with the packed framebuffer
layout in `vesa.h`.

## Building

```bash
make -C src/bootloader/stage2
```

The top-level Makefile passes its absolute `BUILD_DIR` to this Makefile.
Stage2 is compiled as freestanding 32-bit ELF objects and linked with
`linker.ld`; it is not the same binary environment as the 64-bit kernel.
