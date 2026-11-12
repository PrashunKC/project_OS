# project_OS (NBOS)

project_OS is a small x86 operating-system project built from NASM, freestanding
C, and Make. It boots through a BIOS-compatible FAT workflow, loads a kernel,
and provides a 64-bit kernel with interrupt handling, keyboard input, VGA text
output, a framebuffer graphics layer, and an interactive shell.

This repository is an educational kernel rather than a production operating
system. Hardware support is intentionally limited and several interfaces are
implemented for emulators or future expansion.

## Architecture

The normal floppy-image boot path is:

1. BIOS loads `src/bootloader/stage1/boot.asm`.
2. Stage 1 locates `STAGE2.BIN` in the FAT12 root directory and loads it.
3. Stage 2 initializes BIOS disk access, reads `KERNEL.BIN`, and copies it to
   physical address `0x100000`.
4. Stage 2 copies framebuffer metadata to `0x8000` when the real-mode setup
   found a VESA mode.
5. Stage 2 jumps to the kernel entry code.
6. The kernel initializes graphics, the IDT, ISRs, the PIC, and the keyboard,
   then starts the shell.

The kernel sources are configured for x86-64 long mode. The bootloader and
kernel use different build configurations; do not assume that a stage2
freestanding object can be linked with a kernel object.

## Repository map

| Path | Purpose |
| --- | --- |
| [`src/bootloader/`](src/bootloader/) | BIOS boot stages, disk/FAT loading, and VESA setup |
| [`src/bootloader/stage2/`](src/bootloader/stage2/) | C/assembly second-stage loader and low-level helpers |
| [`src/kernel/`](src/kernel/) | 64-bit kernel, interrupts, keyboard, graphics, VGA, and shell |
| [`sdk/`](sdk/) | Headers, startup code, compiler wrapper, and sample NBOS programs |
| [`tools/fat/`](tools/fat/) | Host-side FAT image inspection utility |
| [`build_scripts/`](build_scripts/) | Toolchain checks, setup, examples, and reference notes |
| [`Makefile`](Makefile) | Top-level image and emulator targets |

Component documentation:

- [`src/bootloader/README.md`](src/bootloader/README.md)
- [`src/kernel/README.md`](src/kernel/README.md)
- [`sdk/README.md`](sdk/README.md)
- [`tools/fat/README.md`](tools/fat/README.md)
- [`build_scripts/README.md`](build_scripts/README.md)

## Requirements

The complete build normally needs:

- GNU Make
- NASM
- GCC and binutils (the configured cross-toolchain is preferred)
- `dosfstools` (`mkfs.fat`) and `mtools` (`mcopy`)
- `xorriso` for ISO targets
- QEMU for emulation
- VirtualBox for the `run-vbox*` targets

Run the repository's toolchain check before installing anything:

```bash
./build_scripts/check_toolchain.sh
```

The stage2 build currently invokes `nasm` directly, so a missing NASM
installation prevents even the first assembly object from being produced.

## Build and run

From the repository root:

```bash
make                    # default: floppy image and tools/fat
make clean
make stage2             # build the second-stage loader
make kernel             # build kernel.bin
make floppy_image       # build build/main_floppy.img
./run.sh                # build and boot the floppy image in QEMU
```

Other useful targets are:

```bash
make iso                # floppy image plus SDK on an ISO
make iso_noemul         # no-emulation ISO using the VBR
make hdd_image          # 32 MiB FAT16 hard-disk image
make grub-iso           # GRUB Multiboot ISO for kernel experiments
make qemu-test          # boot kernel.bin directly with QEMU
make qemu-grub          # boot the GRUB ISO with QEMU
```

Build artifacts are written below `build/` and are intentionally not source
inputs. `build.sh` runs a clean build and writes command output to
`build_output.txt`.

## Kernel interfaces

The kernel starts in [`main.c`](src/kernel/main.c) after the assembly entry
code has established the execution environment. Initialization order matters:
graphics and Multiboot data are read first, followed by the IDT, ISR table,
PIC, interrupts, and keyboard driver. The shell consumes keyboard characters
through the interrupt-driven ring buffer.

The graphics layer consumes the packed `FramebufferInfo` structure shared with
stage2. The VGA layer remains the reliable text output path and is used by the
shell and diagnostic messages. Paging is currently established by assembly;
[`paging.c`](src/kernel/paging.c) exposes register readers and a compatibility
initializer rather than allocating page tables at runtime.

## SDK

The SDK is a freestanding 64-bit C interface built around `int 0x80` system
calls. It is separate from the kernel build and currently documents the
intended application ABI; applications require matching kernel syscall support
before they can run as complete user programs.

```bash
source sdk/env.sh
cd sdk/examples/hello
make
```

See [`sdk/README.md`](sdk/README.md) for the syscall table, graphics API, and
known limitations.

## Development notes

- Bootloader structures that mirror on-disk or BIOS data are packed; changing
  their field order or width changes the binary protocol.
- BIOS and hardware entry points are architecture-specific and must preserve
  the calling conventions used by their assembly wrappers.
- The code uses local freestanding headers instead of the host C library in
  bootloader and kernel builds.
- Test scripts and build targets require the relevant emulator/toolchain
  binaries; a successful host-side compile does not prove that the image boots.

## License

See [`LICENSE`](LICENSE).
