#!/bin/bash

# Cross-compiler toolchain setup script for OS development
# This script checks for existing toolchains and builds them if necessary
# Target: i686-elf (32-bit x86 bare metal)

set -e  # Exit on error

# Configuration
TOOLCHAIN_DIR="${HOME}/opt/cross"
TARGET="i686-elf"
BINUTILS_VERSION="2.41"
GCC_VERSION="13.2.0"
BINUTILS_URL="https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.gz"
GCC_URL="https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.gz"
TEMP_DIR="${HOME}/.toolchain_build_tmp"
JOBS=$(nproc)  # Number of parallel jobs for make

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check if toolchain is already installed and working
check_toolchain() {
    print_info "Checking for existing ${TARGET} toolchain..."
    
    local gcc_path="${TOOLCHAIN_DIR}/bin/${TARGET}-gcc"
    local ld_path="${TOOLCHAIN_DIR}/bin/${TARGET}-ld"
    
    if [ -f "${gcc_path}" ] && [ -f "${ld_path}" ]; then
        print_info "Found toolchain at ${TOOLCHAIN_DIR}"
        
        # Verify it works
        if "${gcc_path}" --version >/dev/null 2>&1 && "${ld_path}" --version >/dev/null 2>&1; then
            local gcc_ver=$("${gcc_path}" --version | head -n1)
            local binutils_ver=$("${ld_path}" --version | head -n1)
            print_success "Toolchain is working!"
            echo "  GCC: ${gcc_ver}"
            echo "  Binutils: ${binutils_ver}"
            return 0
        else
            print_warning "Toolchain found but not working properly"
            return 1
        fi
    else
        print_info "Toolchain not found at ${TOOLCHAIN_DIR}"
        return 1
    fi
}

# Check for required build dependencies
check_dependencies() {
    print_info "Checking for required build dependencies..."
    
    local missing_deps=()
    
    # Essential build tools
    local required_tools=("gcc" "g++" "make" "bison" "flex" "gmp" "mpfr" "mpc" "texinfo")
    
    for tool in "${required_tools[@]}"; do
        case "$tool" in
            gmp|mpfr|mpc)
                # Check for development libraries
                if ! pkg-config --exists "$tool" 2>/dev/null; then
                    missing_deps+=("lib${tool}-dev")
                fi
                ;;
            *)
                if ! command_exists "$tool"; then
                    missing_deps+=("$tool")
                fi
                ;;
        esac
    done
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing required dependencies: ${missing_deps[*]}"
        echo ""
        echo "Please install them using your package manager:"
        echo ""
        
        # Detect package manager and provide appropriate command
        if command_exists apt-get; then
            echo "  sudo apt-get install build-essential bison flex libgmp3-dev libmpfr-dev libmpc-dev texinfo"
        elif command_exists dnf; then
            echo "  sudo dnf install gcc gcc-c++ make bison flex gmp-devel mpfr-devel libmpc-devel texinfo"
        elif command_exists pacman; then
            echo "  sudo pacman -S base-devel gmp mpfr libmpc"
        elif command_exists zypper; then
            echo "  sudo zypper install gcc gcc-c++ make bison flex gmp-devel mpfr-devel mpc-devel texinfo"
        else
            echo "  Install: build-essential, bison, flex, libgmp-dev, libmpfr-dev, libmpc-dev, texinfo"
        fi
        
        return 1
    fi
    
    print_success "All dependencies are installed"
    return 0
}

# Download and extract a tarball
download_and_extract() {
    local url=$1
    local filename=$(basename "$url")
    local extract_dir=$2
    
    print_info "Downloading ${filename}..."
    
    if [ -f "${TEMP_DIR}/${filename}" ]; then
        print_info "Using cached ${filename}"
    else
        if ! wget -P "${TEMP_DIR}" "${url}"; then
            print_error "Failed to download ${url}"
            return 1
        fi
    fi
    
    print_info "Extracting ${filename}..."
    if ! tar -xzf "${TEMP_DIR}/${filename}" -C "${TEMP_DIR}"; then
        print_error "Failed to extract ${filename}"
        return 1
    fi
    
    return 0
}

# Build binutils
build_binutils() {
    print_info "Building binutils ${BINUTILS_VERSION} for ${TARGET}..."
    
    local src_dir="${TEMP_DIR}/binutils-${BINUTILS_VERSION}"
    local build_dir="${TEMP_DIR}/build-binutils"
    
    # Download and extract if not already done
    if [ ! -d "${src_dir}" ]; then
        download_and_extract "${BINUTILS_URL}" || return 1
    fi
    
    # Create build directory
    mkdir -p "${build_dir}"
    cd "${build_dir}"
    
    print_info "Configuring binutils..."
    if ! "${src_dir}/configure" \
        --target="${TARGET}" \
        --prefix="${TOOLCHAIN_DIR}" \
        --with-sysroot \
        --disable-nls \
        --disable-werror; then
        print_error "Binutils configuration failed"
        return 1
    fi
    
    print_info "Compiling binutils (this may take several minutes)..."
    if ! make -j"${JOBS}"; then
        print_error "Binutils compilation failed"
        return 1
    fi
    
    print_info "Installing binutils..."
    if ! make install; then
        print_error "Binutils installation failed"
        return 1
    fi
    
    print_success "Binutils built and installed successfully"
    return 0
}

# Build GCC
build_gcc() {
    print_info "Building GCC ${GCC_VERSION} for ${TARGET}..."
    
    local src_dir="${TEMP_DIR}/gcc-${GCC_VERSION}"
    local build_dir="${TEMP_DIR}/build-gcc"
    
    # Download and extract if not already done
    if [ ! -d "${src_dir}" ]; then
        download_and_extract "${GCC_URL}" || return 1
    fi
    
    # Download GCC prerequisites (GMP, MPFR, MPC, ISL)
    print_info "Downloading GCC prerequisites..."
    cd "${src_dir}"
    if ! ./contrib/download_prerequisites; then
        print_warning "Failed to download some prerequisites, but continuing..."
    fi
    
    # Create build directory
    mkdir -p "${build_dir}"
    cd "${build_dir}"
    
    print_info "Configuring GCC..."
    if ! "${src_dir}/configure" \
        --target="${TARGET}" \
        --prefix="${TOOLCHAIN_DIR}" \
        --disable-nls \
        --enable-languages=c,c++ \
        --without-headers; then
        print_error "GCC configuration failed"
        return 1
    fi
    
    print_info "Compiling GCC (this will take a while, possibly 10-30 minutes)..."
    if ! make all-gcc -j"${JOBS}"; then
        print_error "GCC compilation failed"
        return 1
    fi
    
    print_info "Compiling libgcc..."
    if ! make all-target-libgcc -j"${JOBS}"; then
        print_error "libgcc compilation failed"
        return 1
    fi
    
    print_info "Installing GCC..."
    if ! make install-gcc; then
        print_error "GCC installation failed"
        return 1
    fi
    
    print_info "Installing libgcc..."
    if ! make install-target-libgcc; then
        print_error "libgcc installation failed"
        return 1
    fi
    
    print_success "GCC built and installed successfully"
    return 0
}

# Main script execution
main() {
    echo ""
    echo "========================================"
    echo "  Cross-Compiler Toolchain Setup"
    echo "========================================"
    echo ""
    echo "Target: ${TARGET}"
    echo "Installation directory: ${TOOLCHAIN_DIR}"
    echo "Binutils version: ${BINUTILS_VERSION}"
    echo "GCC version: ${GCC_VERSION}"
    echo ""
    
    # Check if toolchain already exists and works
    if check_toolchain; then
        echo ""
        read -p "Toolchain already exists. Rebuild anyway? (y/N): " -n 1 -r
        echo ""
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_info "Using existing toolchain"
            print_info "Add to PATH: export PATH=\"${TOOLCHAIN_DIR}/bin:\$PATH\""
            exit 0
        fi
    fi
    
    # Check dependencies
    if ! check_dependencies; then
        exit 1
    fi
    
    # Create directories
    print_info "Creating directories..."
    mkdir -p "${TOOLCHAIN_DIR}"
    mkdir -p "${TEMP_DIR}"
    
    # Build binutils
    if ! build_binutils; then
        print_error "Failed to build binutils"
        exit 1
    fi
    
    # Add binutils to PATH for GCC build
    export PATH="${TOOLCHAIN_DIR}/bin:${PATH}"
    
    # Build GCC
    if ! build_gcc; then
        print_error "Failed to build GCC"
        exit 1
    fi
    
    # Clean up temporary files
    print_info "Cleaning up temporary files..."
    rm -rf "${TEMP_DIR}"
    
    # Final verification
    echo ""
    echo "========================================"
    if check_toolchain; then
        echo ""
        print_success "Toolchain installation complete!"
        echo ""
        echo "To use the toolchain, add it to your PATH:"
        echo ""
        echo "  export PATH=\"${TOOLCHAIN_DIR}/bin:\$PATH\""
        echo ""
        echo "Or add this line to your ~/.bashrc or ~/.zshrc:"
        echo ""
        echo "  echo 'export PATH=\"${TOOLCHAIN_DIR}/bin:\$PATH\"' >> ~/.bashrc"
        echo ""
        echo "Available tools:"
        echo "  - ${TARGET}-gcc      (C compiler)"
        echo "  - ${TARGET}-g++      (C++ compiler)"
        echo "  - ${TARGET}-ld       (Linker)"
        echo "  - ${TARGET}-as       (Assembler)"
        echo "  - ${TARGET}-objcopy  (Object file converter)"
        echo "  - ${TARGET}-objdump  (Object file disassembler)"
        echo "  - ${TARGET}-ar       (Archive tool)"
        echo ""
    else
        print_error "Toolchain verification failed"
        exit 1
    fi
}

# Run main function
main "$@"
