# Cross-Compiler Toolchain Integration Example

## Quick Usage Guide

### First Time Setup

1. **Run the toolchain setup script:**
   ```bash
   cd /home/kde/Pictures/project_OS
   ./build_scripts/setup_toolchain.sh
   ```

2. **Add toolchain to PATH** (add to `~/.bashrc` for permanent):
   ```bash
   export PATH="$HOME/opt/cross/bin:$PATH"
   ```

3. **Verify installation:**
   ```bash
   i686-elf-gcc --version
   i686-elf-ld --version
   ```

### Integrating with Your Makefile

#### Option 1: Include toolchain.mk (Recommended)

Add this line near the top of your main Makefile:

```makefile
# Include cross-compiler toolchain configuration
include build_scripts/toolchain.mk

# Your existing configuration
ASM=nasm
CC=gcc
CC16=wcc
LD16=wlink
SRC_DIR=src
TOOLS_DIR=tools
BUILD_DIR=build

# Rest of your Makefile...
```

Then you can use these make targets:

```bash
# Check if toolchain is installed
make check-toolchain

# Setup toolchain if not installed
make setup-toolchain

# Show toolchain info
make toolchain-info

# Build your OS (will use existing tools)
make all
```

#### Option 2: Add Toolchain Check to Your Build

Add a toolchain check before building kernel or protected mode code:

```makefile
# Include toolchain
include build_scripts/toolchain.mk

# Example: Building a 32-bit kernel (future use)
kernel32: check-toolchain
	$(CC_CROSS) $(CFLAGS_CROSS) -c src/kernel/kernel32.c -o $(BUILD_DIR)/kernel32.o
	$(LD_CROSS) $(LDFLAGS_CROSS) -T src/kernel/linker.ld -o $(BUILD_DIR)/kernel32.bin $(BUILD_DIR)/kernel32.o
```

#### Option 3: Manual Detection

If you want custom behavior, you can detect the toolchain manually:

```makefile
# Detect cross-compiler
CROSS_CC := $(shell command -v i686-elf-gcc 2>/dev/null)

ifeq ($(CROSS_CC),)
    $(error Cross-compiler not found. Run: ./build_scripts/setup_toolchain.sh)
endif

# Use cross-compiler for kernel
kernel:
	$(CROSS_CC) -ffreestanding -c kernel.c -o kernel.o
```

## Example: Full Makefile with Toolchain Support

```makefile
# Include cross-compiler toolchain
include build_scripts/toolchain.mk

# Existing tools
ASM=nasm
CC=gcc
CC16=wcc
LD16=wlink
SRC_DIR=src
TOOLS_DIR=tools
BUILD_DIR=build

.PHONY: all floppy_image bootloader kernel32 clean

# Default target
all: floppy_image

# Floppy image with everything
floppy_image: $(BUILD_DIR)/main_floppy.img

$(BUILD_DIR)/main_floppy.img: bootloader kernel32
	dd if=/dev/zero of=$(BUILD_DIR)/main_floppy.img bs=512 count=2880
	mkfs.fat -F 12 -n "NBOS" $(BUILD_DIR)/main_floppy.img
	dd if=$(BUILD_DIR)/stage1.bin of=$(BUILD_DIR)/main_floppy.img conv=notrunc
	mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/stage2.bin "::/stage2.bin"
	mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/kernel32.bin "::/kernel32.bin"

# Bootloader (16-bit, uses NASM and Watcom)
bootloader: $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin

$(BUILD_DIR)/stage1.bin: always
	$(ASM) $(SRC_DIR)/bootloader/stage1/boot.asm -f bin -o $@

$(BUILD_DIR)/stage2.bin: always
	$(MAKE) -C $(SRC_DIR)/bootloader/stage2 BUILD_DIR=$(abspath $(BUILD_DIR))

# 32-bit protected mode kernel (uses cross-compiler)
kernel32: check-toolchain $(BUILD_DIR)/kernel32.bin

$(BUILD_DIR)/kernel32.bin: $(SRC_DIR)/kernel/kernel32.c always
	$(CC_CROSS) $(CFLAGS_CROSS) -c $(SRC_DIR)/kernel/kernel32.c -o $(BUILD_DIR)/kernel32.o
	$(LD_CROSS) $(LDFLAGS_CROSS) -T $(SRC_DIR)/kernel/linker.ld -o $@ $(BUILD_DIR)/kernel32.o

# Utilities
always:
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)/*

# Show help
help:
	@echo "Available targets:"
	@echo "  all              - Build everything (default)"
	@echo "  floppy_image     - Create bootable floppy disk image"
	@echo "  bootloader       - Build 16-bit bootloader"
	@echo "  kernel32         - Build 32-bit kernel (requires cross-compiler)"
	@echo "  clean            - Remove build artifacts"
	@echo ""
	@echo "Toolchain targets:"
	@echo "  check-toolchain  - Verify cross-compiler is installed"
	@echo "  setup-toolchain  - Install cross-compiler toolchain"
	@echo "  toolchain-info   - Show toolchain configuration"
```

## What Each Tool Is Used For

### Current Setup (16-bit Real Mode)
- **NASM** (`nasm`) - Assembles bootloader assembly code
- **Open Watcom C** (`wcc`) - Compiles 16-bit C code for Stage 2 bootloader
- **Open Watcom Linker** (`wlink`) - Links 16-bit object files
- **GCC** (`gcc`) - Compiles utilities (FAT reader) for host system

### Cross-Compiler (32-bit Protected Mode)
When you transition to 32-bit protected mode, you'll need:
- **i686-elf-gcc** - Compiles 32-bit C code for bare metal
- **i686-elf-ld** - Links 32-bit object files with custom linker scripts
- **i686-elf-as** - Assembles 32-bit assembly code
- **i686-elf-objcopy** - Converts object files (e.g., ELF to binary)

### Why You Need Both

```
┌─────────────────────────────────────────────────────────────┐
│  Boot Process Flow                                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  BIOS (16-bit real mode)                                    │
│    ↓                                                        │
│  Stage 1 Bootloader (NASM) ← 16-bit assembly               │
│    ↓                                                        │
│  Stage 2 Bootloader (Watcom C) ← 16-bit C + assembly       │
│    ↓                                                        │
│  Switch to Protected Mode ← assembly code                   │
│    ↓                                                        │
│  32-bit Kernel (i686-elf-gcc) ← 32-bit C + assembly        │
│    ↓                                                        │
│  Operating System                                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## When to Use the Cross-Compiler

You'll need the cross-compiler when you:

1. **Switch to 32-bit protected mode**
   - Can't use Open Watcom (16-bit only)
   - Need proper 32-bit code generation
   - Want to use modern C/C++ features

2. **Write the kernel**
   - Kernel runs in protected mode
   - Needs to be compiled as bare metal (no OS)
   - Requires custom memory layout (linker scripts)

3. **Implement advanced features**
   - Memory management (paging, virtual memory)
   - Multitasking
   - Device drivers
   - File systems

## Example Workflow

### Current Project (16-bit Only)
```bash
make all          # Uses NASM + Watcom
./run.sh          # Boot and run
```

### Future Project (16-bit + 32-bit)
```bash
# First time: setup cross-compiler
make setup-toolchain

# Regular builds
make check-toolchain    # Verify toolchain
make all               # Build bootloader (Watcom) + kernel (GCC cross)
./run.sh               # Boot and run
```

## Troubleshooting

### Error: "i686-elf-gcc: command not found"

**Solution:**
```bash
# Install the toolchain
./build_scripts/setup_toolchain.sh

# Add to PATH
export PATH="$HOME/opt/cross/bin:$PATH"

# Or add to ~/.bashrc permanently
echo 'export PATH="$HOME/opt/cross/bin:$PATH"' >> ~/.bashrc
```

### Error: "Missing dependencies"

**Solution (Ubuntu/Debian):**
```bash
sudo apt-get install build-essential bison flex libgmp3-dev libmpfr-dev libmpc-dev texinfo wget
```

**Solution (Fedora/RHEL):**
```bash
sudo dnf install gcc gcc-c++ make bison flex gmp-devel mpfr-devel libmpc-devel texinfo wget
```

### Script takes too long

This is normal! Building GCC from source takes 10-30 minutes. The script:
- Downloads ~200 MB of source code
- Compiles thousands of files
- Uses all CPU cores to speed up the process

### Want to use system packages instead?

**Arch Linux:**
```bash
yay -S i686-elf-gcc i686-elf-binutils
```

**macOS with Homebrew:**
```bash
brew install i686-elf-gcc
brew install i686-elf-binutils
```

Then just verify:
```bash
make check-toolchain
```

## Additional Resources

- **Setup Script**: `build_scripts/setup_toolchain.sh`
- **Makefile Include**: `build_scripts/toolchain.mk`
- **Documentation**: `build_scripts/README.md`
- **OSDev Wiki**: https://wiki.osdev.org/GCC_Cross-Compiler
