# Toolchain Setup - Quick Reference

## TL;DR - Get Started in 3 Steps

```bash
# 1. Build and install the toolchain (10-30 minutes)
./build_scripts/setup_toolchain.sh

# 2. Add to PATH
export PATH="$HOME/opt/cross/bin:$PATH"

# 3. Verify it works
./build_scripts/check_toolchain.sh
```

## What Was Created

Your `build_scripts/` directory now contains:

1. **`setup_toolchain.sh`** - Automated toolchain installer
   - Downloads GCC 13.2.0 and Binutils 2.41
   - Builds for i686-elf target (32-bit x86 bare metal)
   - Installs to `~/opt/cross`
   - Takes 10-30 minutes to complete

2. **`check_toolchain.sh`** - Quick verification script
   - Checks if toolchain is installed
   - Verifies it works correctly
   - Shows available tools

3. **`toolchain.mk`** - Makefile integration
   - Include in your Makefile
   - Provides `check-toolchain`, `setup-toolchain` targets
   - Sets up cross-compiler variables

4. **`README.md`** - Full documentation
   - Detailed setup instructions
   - Troubleshooting guide
   - Customization options

5. **`USAGE_EXAMPLES.md`** - Integration examples
   - How to use with your Makefile
   - Real-world examples
   - When to use which compiler

## Quick Commands

### Check Status
```bash
./build_scripts/check_toolchain.sh
```

### Install Toolchain
```bash
./build_scripts/setup_toolchain.sh
```

### Makefile Integration

Add to your `Makefile`:
```makefile
include build_scripts/toolchain.mk
```

Then use:
```bash
make check-toolchain    # Verify installation
make setup-toolchain    # Install if needed
make toolchain-info     # Show configuration
```

## What Gets Installed

**Location:** `~/opt/cross/`

**Tools:**
- `i686-elf-gcc` - C compiler
- `i686-elf-g++` - C++ compiler  
- `i686-elf-as` - Assembler
- `i686-elf-ld` - Linker
- `i686-elf-ar` - Archive tool
- `i686-elf-objcopy` - Object file converter
- `i686-elf-objdump` - Disassembler
- `i686-elf-nm` - Symbol viewer
- `i686-elf-strip` - Debug symbol remover
- `i686-elf-ranlib` - Archive indexer

## Why You Need This

Your current project uses:
- **NASM** - For bootloader assembly (16-bit real mode)
- **Open Watcom C** - For Stage 2 bootloader C code (16-bit)

You'll need the **cross-compiler** when you:
- Switch to 32-bit protected mode
- Write the kernel in C/C++
- Need modern compiler features
- Want to use standard development tools

## Compilation Flow

```
16-bit Bootloader                 32-bit Kernel
(Current)                         (Future)
     │                                 │
     ├─ Stage 1 ────── NASM           │
     │                                 │
     └─ Stage 2 ────── Watcom C       │
                                       │
                       Switch to 32-bit
                             │
                             ├─ Kernel ─────── i686-elf-gcc
                             │
                             └─ Drivers ────── i686-elf-gcc
```

## System Requirements

**Disk Space:** ~5 GB during build, ~500 MB after installation

**Build Time:** 10-30 minutes (uses all CPU cores)

**Dependencies:**
- build-essential (gcc, g++, make)
- bison, flex
- libgmp-dev, libmpfr-dev, libmpc-dev
- texinfo, wget

## Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential bison flex libgmp3-dev libmpfr-dev libmpc-dev texinfo wget
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc gcc-c++ make bison flex gmp-devel mpfr-devel libmpc-devel texinfo wget
```

**Arch Linux:**
```bash
sudo pacman -S base-devel gmp mpfr libmpc wget
```

## Troubleshooting

### "Command not found"
```bash
export PATH="$HOME/opt/cross/bin:$PATH"
```

### Build fails
Check you have all dependencies installed (see above).

### Takes too long
This is normal. Building GCC from source is a lengthy process.

### Permission denied
```bash
chmod +x build_scripts/*.sh
```

## Alternative Installation Methods

### Arch Linux (AUR)
```bash
yay -S i686-elf-gcc i686-elf-binutils
```

### macOS (Homebrew)
```bash
brew install i686-elf-gcc i686-elf-binutils
```

### Pre-built Binaries
Some distributions provide pre-built cross-compiler packages. Check your package manager first before building from source.

## Files Overview

```
build_scripts/
├── setup_toolchain.sh      # Main installer (run this first)
├── check_toolchain.sh      # Verify installation
├── toolchain.mk            # Makefile include
├── README.md               # Full documentation
├── USAGE_EXAMPLES.md       # Integration examples
└── QUICK_REFERENCE.md      # This file
```

## Next Steps

1. **First time setup:**
   ```bash
   ./build_scripts/setup_toolchain.sh
   ```

2. **Add to PATH permanently:**
   ```bash
   echo 'export PATH="$HOME/opt/cross/bin:$PATH"' >> ~/.bashrc
   source ~/.bashrc
   ```

3. **Verify:**
   ```bash
   i686-elf-gcc --version
   ```

4. **Start using in your project:**
   - See `USAGE_EXAMPLES.md` for integration
   - Add `include build_scripts/toolchain.mk` to Makefile
   - Use `$(CC_CROSS)` variable for cross-compilation

## Resources

- **OSDev Wiki:** https://wiki.osdev.org/GCC_Cross-Compiler
- **GCC Manual:** https://gcc.gnu.org/onlinedocs/
- **Binutils Manual:** https://sourceware.org/binutils/docs/

## Support

If you encounter issues:
1. Check `README.md` for detailed troubleshooting
2. Run `./build_scripts/check_toolchain.sh` to diagnose
3. Review `USAGE_EXAMPLES.md` for integration help
4. Check OSDev Wiki for community solutions

---

**Pro Tip:** Run `make setup-toolchain` from your main Makefile for automatic installation!
