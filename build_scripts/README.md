# Build Scripts

This directory contains build scripts and configuration for the OS development toolchain.

## Overview

For OS development, you need a cross-compiler toolchain that targets bare metal (no operating system). This project uses the `i686-elf` target, which is a 32-bit x86 cross-compiler suitable for building operating systems.

## Quick Start

### 1. Setup Toolchain (First Time Only)

Run the automated setup script:

```bash
./build_scripts/setup_toolchain.sh
```

This will:
- Check for required dependencies
- Download GCC and binutils source code
- Build the cross-compiler for `i686-elf` target
- Install to `~/opt/cross`

**Note:** Building the toolchain takes 10-30 minutes depending on your system.

### 2. Add Toolchain to PATH

After installation, add the toolchain to your PATH:

```bash
export PATH="$HOME/opt/cross/bin:$PATH"
```

To make this permanent, add it to your `~/.bashrc`:

```bash
echo 'export PATH="$HOME/opt/cross/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### 3. Verify Installation

```bash
i686-elf-gcc --version
i686-elf-ld --version
```

## Using with Makefile

The main project Makefile can integrate with the toolchain automatically.

### Add to Your Makefile

At the top of your main `Makefile`, include:

```makefile
include build_scripts/toolchain.mk
```

### Available Make Targets

Once included, you can use these targets:

```bash
# Check if toolchain is installed
make check-toolchain

# Setup toolchain automatically
make setup-toolchain

# Show toolchain information
make toolchain-info

# Show help for toolchain targets
make help-toolchain
```

### Example Integration

```makefile
# Include toolchain configuration
include build_scripts/toolchain.mk

# Your build targets
kernel: check-toolchain
	$(CC_CROSS) $(CFLAGS_CROSS) -c kernel.c -o kernel.o
	$(LD_CROSS) $(LDFLAGS_CROSS) -T linker.ld -o kernel.bin kernel.o
```

## Files

### `setup_toolchain.sh`

Automated script to download, build, and install the cross-compiler toolchain.

**Features:**
- Detects if toolchain already exists
- Checks for required build dependencies
- Downloads GNU binutils and GCC from official mirrors
- Builds for `i686-elf` target
- Parallel compilation (uses all CPU cores)
- Verifies successful installation

**Configuration:**
- Installation directory: `~/opt/cross`
- Target: `i686-elf` (32-bit x86 bare metal)
- Binutils version: 2.41
- GCC version: 13.2.0

**Dependencies:**
- build-essential (gcc, g++, make)
- bison
- flex
- libgmp-dev
- libmpfr-dev
- libmpc-dev
- texinfo

### `toolchain.mk`

Makefile include for toolchain integration.

**Provides:**
- `CC_CROSS`, `LD_CROSS`, etc. - Cross-compiler tool variables
- `CFLAGS_CROSS`, `LDFLAGS_CROSS` - Recommended compiler flags for OS development
- `check-toolchain` - Verify toolchain is available
- `setup-toolchain` - Run setup script
- `toolchain-info` - Display configuration

**Variables:**
- `TOOLCHAIN_DIR` - Installation directory (default: `~/opt/cross`)
- `TARGET_PREFIX` - Target triplet (default: `i686-elf`)

## Manual Installation

If you prefer to install the toolchain manually or have an existing installation:

1. Set `TOOLCHAIN_DIR` to your installation directory
2. Ensure `i686-elf-gcc` and related tools are in PATH
3. Run `make check-toolchain` to verify

### Alternative Package Managers

Some distributions provide cross-compiler packages:

**Arch Linux:**
```bash
yay -S i686-elf-gcc i686-elf-binutils
```

**Homebrew (macOS):**
```bash
brew install i686-elf-gcc
brew install i686-elf-binutils
```

## Troubleshooting

### "Command not found" errors

Make sure the toolchain is in your PATH:
```bash
export PATH="$HOME/opt/cross/bin:$PATH"
```

### Build fails with missing dependencies

Install required packages:

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential bison flex libgmp3-dev libmpfr-dev libmpc-dev texinfo
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc gcc-c++ make bison flex gmp-devel mpfr-devel libmpc-devel texinfo
```

**Arch Linux:**
```bash
sudo pacman -S base-devel gmp mpfr libmpc
```

### Compilation is very slow

The script uses all available CPU cores by default. Building GCC typically takes 10-30 minutes on modern systems.

### Out of disk space

Building the toolchain requires about 3-5 GB of free disk space:
- Source downloads: ~200 MB
- Build directory: ~2-3 GB (deleted after installation)
- Installation: ~500 MB

### Permission denied errors

If installing to a system directory, you may need sudo:
```bash
sudo ./build_scripts/setup_toolchain.sh
```

Or change `TOOLCHAIN_DIR` in the script to a user-writable location.

## Customization

### Change Installation Directory

Edit `setup_toolchain.sh`:
```bash
TOOLCHAIN_DIR="/usr/local/cross"  # Or any directory you prefer
```

### Change Target Architecture

For 64-bit x86:
```bash
TARGET="x86_64-elf"
```

For ARM:
```bash
TARGET="arm-none-eabi"
```

### Use Different Versions

Edit version numbers in `setup_toolchain.sh`:
```bash
BINUTILS_VERSION="2.40"
GCC_VERSION="12.3.0"
```

## Additional Resources

- [OSDev Wiki - GCC Cross-Compiler](https://wiki.osdev.org/GCC_Cross-Compiler)
- [GNU Binutils Documentation](https://sourceware.org/binutils/docs/)
- [GCC Documentation](https://gcc.gnu.org/onlinedocs/)
- [i686 Target Information](https://wiki.osdev.org/Target_Triplet)

## License

The toolchain setup scripts are provided as-is for OS development purposes. GCC and binutils are licensed under the GPL.
