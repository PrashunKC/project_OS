# Build system summary

The root Makefile composes four kinds of output:

1. BIOS boot sectors (`stage1`, `mbr`, and `vbr`) assembled as flat binaries.
2. The C/assembly stage2 loader linked as `stage2.bin`.
3. The freestanding x86-64 kernel linked as `kernel.elf` and converted to
   `kernel.bin`.
4. FAT/ISO/hard-disk images and the host-side FAT inspection tool.

The default `all` target builds `floppy_image` and `tools_fat`. The floppy
image contains the stage1 boot sector plus `STAGE2.BIN` and `KERNEL.BIN`
files in a FAT12 filesystem. The ISO and hard-disk targets arrange the same
components using their respective boot layouts.

`build_scripts/toolchain.mk` provides tool discovery and convenience targets.
It is included by the root and stage2 Makefiles; the kernel Makefile overrides
the compiler/linker selection for its 64-bit build.
