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
echo "Starting QEMU..."
# Use -d guest_errors to see if we trigger any CPU exceptions or invalid hardware access
qemu-system-i386 -fda build/main_floppy.img -d guest_errors
