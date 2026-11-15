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
- [`docs/DETAILED_CODEBASE_GUIDE.md`](docs/DETAILED_CODEBASE_GUIDE.md)
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

VirtualBox cannot share AMD-V/VT-x with an active KVM host. The VirtualBox
targets attempt to unload inactive KVM modules automatically using `sudo`; if
KVM is busy, use the QEMU targets or inspect `/dev/kvm` ownership first.
The floppy `run-vbox` target deliberately disables VirtualBox hardware
virtualization because this 16-bit-to-64-bit VESA boot path can triple-fault
under VirtualBox's accelerated execution mode. The ISO and GRUB targets retain
hardware virtualization.

The `grub-iso` target creates a BIOS-bootable Multiboot ISO using the linked
`kernel.elf` image and requires `grub-pc-bin` in addition to `grub-mkrescue`
and `xorriso`. Its GRUB configuration intentionally keeps the boot menu in
text mode for compatibility; the kernel consumes either the standard
Multiboot framebuffer fields or the legacy VBE mode-information block when
supplied by GRUB.

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
make iso_noemul         # ISO using the known-good floppy boot image
make hdd_image          # 32 MiB FAT16 hard-disk image
make grub-iso           # GRUB Multiboot ISO for kernel experiments
make qemu-test          # boot the custom floppy path with QEMU
make qemu-grub          # boot the GRUB ISO with QEMU
make test               # full non-interactive build/image/QEMU diagnostics
make requirements       # list requirements by build/test target
```

`./test.sh` (or `make test`) writes a timestamped report to
`build/diagnostics/`. It records tool and virtualization state, builds the
supported images, validates the FAT and Multiboot artifacts, observes floppy
and GRUB boots with bounded QEMU runs, and captures monitor screenshots and
QEMU diagnostic logs, including pre- and post-welcome terminal screenshots.
It also validates the `qemu-test`, `iso_noemul`, and GRUB Make recipes and
checks that the linked kernel entry address matches stage2's handoff offset.
Raw output remains in `test.log`; interpreted suspicious evidence is copied
to `warnings.txt`, proof-check failures to `errors.txt`, and the final counts
to `summary.txt`. Warnings are visible but do not fail a run unless a proof
check fails. Expected QEMU observation timeouts are reported separately from
boot errors.
Use `./test.sh --virtualbox` to collect VirtualBox host information, or
`./test.sh --virtualbox-boot` to start the `run-vbox` target and fail if the
VM fails to remain running, produces a VirtualBox fault signature, or cannot
produce a screen capture. The latter stores the VM information, VirtualBox
log, boot output, and screenshot, then powers off the test VM afterward.

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
