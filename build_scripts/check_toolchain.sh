#!/bin/bash
# Quick check script to verify if toolchain is installed

TOOLCHAIN_DIR="${HOME}/opt/cross"
TARGET="i686-elf"

echo "Checking for ${TARGET} cross-compiler toolchain..."
echo ""

# Check if toolchain directory exists
if [ ! -d "${TOOLCHAIN_DIR}" ]; then
    echo "❌ Toolchain directory not found: ${TOOLCHAIN_DIR}"
    echo ""
    echo "Run: ./build_scripts/setup_toolchain.sh"
    exit 1
fi

# Check for GCC
GCC_PATH="${TOOLCHAIN_DIR}/bin/${TARGET}-gcc"
if [ -f "${GCC_PATH}" ]; then
    echo "✓ GCC found: ${GCC_PATH}"
    "${GCC_PATH}" --version | head -n1
else
    echo "❌ GCC not found: ${GCC_PATH}"
    exit 1
fi

echo ""

# Check for binutils
LD_PATH="${TOOLCHAIN_DIR}/bin/${TARGET}-ld"
if [ -f "${LD_PATH}" ]; then
    echo "✓ Binutils found: ${LD_PATH}"
    "${LD_PATH}" --version | head -n1
else
    echo "❌ Binutils not found: ${LD_PATH}"
    exit 1
fi

echo ""

# Check if in PATH
if command -v "${TARGET}-gcc" >/dev/null 2>&1; then
    echo "✓ Toolchain is in PATH"
else
    echo "⚠ Toolchain not in PATH"
    echo ""
    echo "Add to PATH with:"
    echo "  export PATH=\"${TOOLCHAIN_DIR}/bin:\$PATH\""
    echo ""
    echo "Or add to ~/.bashrc:"
    echo "  echo 'export PATH=\"${TOOLCHAIN_DIR}/bin:\$PATH\"' >> ~/.bashrc"
fi

echo ""
echo "Available tools:"
for tool in gcc g++ as ld ar objcopy objdump nm strip ranlib; do
    tool_path="${TOOLCHAIN_DIR}/bin/${TARGET}-${tool}"
    if [ -f "${tool_path}" ]; then
        echo "  - ${TARGET}-${tool}"
    fi
done

echo ""
echo "✓ Toolchain is ready!"

echo ""
echo "========================================"
echo "Checking VirtualBox..."
echo "========================================"
echo ""

# Check for VirtualBox
if command -v VBoxManage >/dev/null 2>&1; then
    echo "✓ VirtualBox found"
    VBoxManage --version
else
    echo "❌ VirtualBox not found"
    echo ""
    echo "Install with: sudo apt-get install virtualbox"
    echo "Or run: ./build_scripts/setup_toolchain.sh"
fi

echo ""

# Check if vboxdrv module is loaded
if lsmod | grep -q vboxdrv; then
    echo "✓ VirtualBox kernel modules loaded"
else
    echo "⚠ VirtualBox kernel modules not loaded"
    echo "  Run: sudo modprobe vboxdrv"
fi

echo ""

# Check if user is in vboxusers group
if groups | grep -q vboxusers; then
    echo "✓ User is in vboxusers group"
else
    echo "⚠ User not in vboxusers group"
    echo "  Run: sudo usermod -aG vboxusers $USER"
    echo "  Then log out and back in"
fi

echo ""
echo "========================================"
echo "Checking QEMU..."
echo "========================================"
echo ""

# Check for QEMU
if command -v qemu-system-i386 >/dev/null 2>&1; then
    echo "✓ QEMU found"
    qemu-system-i386 --version | head -n1
else
    echo "⚠ QEMU not found (optional)"
    echo "  Install with: sudo apt-get install qemu-system-x86"
fi

echo ""
echo "========================================"
echo "Summary"
echo "========================================"
echo ""
echo "Run targets:"
echo "  make run-vbox      - Run in VirtualBox (floppy)"
echo "  make run-vbox-iso  - Run in VirtualBox (ISO)"
echo "  ./run.sh           - Run in QEMU"
echo ""
