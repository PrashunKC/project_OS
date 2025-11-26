#!/bin/bash
# Run the OS in QEMU

# Exit on error
set -e

# Check for QEMU
if ! command -v qemu-system-i386 &> /dev/null; then
    echo "Error: qemu-system-i386 not found. Please install QEMU."
    exit 1
fi

# Build the project
echo "Building project..."
make

# Check if floppy image exists
if [ ! -f build/main_floppy.img ]; then
    echo "Error: build/main_floppy.img not found!"
    exit 1
fi

# Run QEMU
# Unset Snap-related environment variables that might interfere with QEMU
unset GTK_PATH
unset GTK_EXE_PREFIX
unset GIO_MODULE_DIR
unset GSETTINGS_SCHEMA_DIR

echo "Starting QEMU..."
qemu-system-i386 -fda build/main_floppy.img
