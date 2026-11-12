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

## Troubleshooting

- `nasm: No such file or directory`: install NASM before building assembly.
- `mkfs.fat` or `mcopy` missing: install `dosfstools` and `mtools`.
- `qemu-system-x86_64` missing: install QEMU's x86 system package.
- Cross-toolchain errors: run `make toolchain-info`; remember that the kernel
  currently selects host x86-64 GCC/LD explicitly.
