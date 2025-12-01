#!/bin/bash
# NBOS SDK Environment Setup
# Source this file: source env.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export NBOS_SDK="$SCRIPT_DIR"
export NBOS_INCLUDE="$NBOS_SDK/include"
export NBOS_LIB="$NBOS_SDK/lib"

# Add tools to PATH
export PATH="$NBOS_SDK/tools:$PATH"

echo "NBOS SDK environment configured:"
echo "  NBOS_SDK=$NBOS_SDK"
echo "  NBOS_INCLUDE=$NBOS_INCLUDE"
echo ""
echo "Use 'nbos-gcc' to compile programs for NBOS"
