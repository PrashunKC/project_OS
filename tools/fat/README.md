# FAT image inspection utility

`fat.c` is a host-side command-line utility for reading a file from a FAT
disk image. It is independent of the freestanding bootloader FAT driver and
uses the host C library for file I/O and memory allocation.

## Usage

```text
fat <disk image> <11-character FAT file name>
```

For example:

```bash
make tools_fat
build/tools/fat build/main_floppy.img KERNEL  BIN
```

The input name is compared against the raw 11-byte FAT directory name, so
callers must provide the directory's padded 8.3 representation rather than a
normal dotted filename. The utility reads the boot sector, the first FAT,
and the root directory, follows the file's cluster chain, and prints readable
bytes while escaping other bytes as hexadecimal.

The tool is diagnostic code for FAT12-style images; it is not a general FAT
implementation and should not be used as the bootloader's runtime library.
