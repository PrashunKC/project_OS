# Build scripts and toolchain helpers

This directory contains optional cross-toolchain helpers and reference
material. The repository's actual build graph is defined by the root
[`Makefile`](../Makefile) and the component Makefiles.

## Files

- `toolchain.mk` detects an `i686-elf` toolchain under `$HOME/opt/cross` and
  falls back to host GCC/binutils when a cross compiler is unavailable.
- `check_toolchain.sh` checks the configured cross compiler, VirtualBox, and
  QEMU installations.
- `setup_toolchain.sh` downloads and builds the configured `i686-elf`
  binutils/GCC pair. It may also install/configure VirtualBox and therefore
  should be reviewed before running with elevated privileges.
- `QUICK_REFERENCE.md`, `SUMMARY.md`, and `USAGE_EXAMPLES.md` are command
  references for the current targets and should be read alongside the root
  Makefile.

## Common commands

```bash
make check-toolchain
make toolchain-info
./build_scripts/check_toolchain.sh
./build_scripts/setup_toolchain.sh
```

The current kernel Makefile explicitly uses host `gcc`/`ld` with x86-64
flags, while stage2 uses the shared cross-tool variables and 32-bit ELF
objects. A successful cross-toolchain check is useful for stage2 but does not
replace NASM, FAT image tools, QEMU, or xorriso.
