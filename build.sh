#!/bin/bash
# Build script for project_OS
# Navigate to the script's directory to ensure relative paths work
cd "$(dirname "$0")"

make clean
make > build_output.txt 2>&1
