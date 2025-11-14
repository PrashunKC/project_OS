#!/bin/bash
# Run the OS in QEMU

# Build first
make

# Check if build succeeded
if [ ! -f build/main_floppy.img ]; then
    echo "Build failed - main_floppy.img not found"
    exit 1
fi

# Run in QEMU
echo "Starting QEMU..."
qemu-system-i386 -fda build/main_floppy.img