# Build quick reference

Run commands from the repository root.

| Command | Result |
| --- | --- |
| `make` | Build the floppy image and host FAT utility |
| `make clean` | Remove generated artifacts under `build/` |
| `make stage2` | Build `build/stage2.bin` |
| `make kernel` | Build `build/kernel.bin` |
| `make floppy_image` | Build `build/main_floppy.img` |
| `make iso` | Build a bootable ISO containing the floppy image and SDK |
| `make iso_noemul` | Build a no-emulation ISO |
| `make hdd_image` | Build a 32 MiB FAT16 hard-disk image |
| `make grub-iso` | Build the GRUB/Multiboot kernel ISO |
| `make tools_fat` | Build `build/tools/fat` |
| `./run.sh` | Build and boot the floppy image with QEMU |

Required tools vary by target. The first stage2 assembly step requires NASM;
image targets additionally require `mkfs.fat`, `mcopy`, and often `xorriso`.
