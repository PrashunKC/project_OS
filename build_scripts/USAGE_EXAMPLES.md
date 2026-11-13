# Build examples

## Build and inspect a floppy image

```bash
make floppy_image
build/tools/fat build/main_floppy.img "KERNEL  BIN"
```

The FAT utility expects the raw 11-byte, space-padded 8.3 directory name.

## Run under QEMU

```bash
./run.sh
```

For a complete non-interactive validation run, including tool discovery,
all image targets, Multiboot/FAT checks, bounded QEMU observations, monitor
screenshots, and host diagnostics:

```bash
./test.sh
```

The run creates a timestamped bundle under `build/diagnostics/` containing
`test.log`, `summary.txt`, `warnings.txt`, `errors.txt`, tool and
virtualization reports, QEMU logs, and PPM screenshots. The warning analyzer
reports suspicious log signatures separately from confirmed proof failures
without hiding the original command output. Useful options are `./test.sh --clean`,
`./test.sh --no-optional`, and `./test.sh --virtualbox`. The script never
unloads KVM modules or changes VirtualBox state.
Use `./test.sh --virtualbox-boot` when QEMU-only evidence is insufficient; it
starts the custom floppy VM, verifies that VirtualBox reaches a running state,
captures the display, scans the VM log for fault signatures, and powers it off.

To print the dependency matrix without building anything:

```bash
make requirements
```

To build separately and boot a specific image:

```bash
make floppy_image
qemu-system-x86_64 -fda build/main_floppy.img
```

## Build alternate images

```bash
make iso
make iso_noemul
make hdd_image
make grub-iso
```

`run-vbox`, `run-vbox-iso`, and `run-grub-vbox` create or replace named
VirtualBox VMs. Use them only when VirtualBox is installed and the image
target has completed successfully.

VirtualBox cannot share AMD-V/VT-x with KVM. The VirtualBox targets attempt to
unload inactive KVM modules automatically using `sudo`; if KVM is busy, use
`sudo fuser -v /dev/kvm` to identify the owner or use `make qemu-grub`.

The BIOS GRUB ISO target also requires the BIOS GRUB modules:
`sudo apt install grub-pc-bin`.

## Troubleshooting

- `nasm: No such file or directory`: install NASM before building assembly.
- `mkfs.fat` or `mcopy` missing: install `dosfstools` and `mtools`.
- `qemu-system-x86_64` missing: install QEMU's x86 system package.
- Cross-toolchain errors: run `make toolchain-info`; remember that the kernel
  currently selects host x86-64 GCC/LD explicitly.
