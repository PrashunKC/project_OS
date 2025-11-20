# Toolchain detection and setup for cross-compilation
# Include this file in your main Makefile

# Toolchain configuration
TOOLCHAIN_DIR ?= $(HOME)/opt/cross
TARGET_PREFIX ?= i686-elf
TOOLCHAIN_SETUP_SCRIPT := $(shell dirname $(lastword $(MAKEFILE_LIST)))/setup_toolchain.sh

# Toolchain binaries
export PATH := $(TOOLCHAIN_DIR)/bin:$(PATH)

# Cross-compiler tools
CC_CROSS = $(TARGET_PREFIX)-gcc
CXX_CROSS = $(TARGET_PREFIX)-g++
AS_CROSS = $(TARGET_PREFIX)-as
LD_CROSS = $(TARGET_PREFIX)-ld
AR_CROSS = $(TARGET_PREFIX)-ar
OBJCOPY = $(TARGET_PREFIX)-objcopy
OBJDUMP = $(TARGET_PREFIX)-objdump

# Check if toolchain is available
TOOLCHAIN_GCC := $(shell command -v $(CC_CROSS) 2>/dev/null)
TOOLCHAIN_LD := $(shell command -v $(LD_CROSS) 2>/dev/null)

# Colors for output
COLOR_BLUE := \033[0;34m
COLOR_GREEN := \033[0;32m
COLOR_YELLOW := \033[1;33m
COLOR_RED := \033[0;31m
COLOR_RESET := \033[0m

# Check if toolchain exists
.PHONY: check-toolchain
check-toolchain:
	@if [ -z "$(TOOLCHAIN_GCC)" ] || [ -z "$(TOOLCHAIN_LD)" ]; then \
		echo "$(COLOR_RED)Error: Cross-compiler toolchain not found!$(COLOR_RESET)"; \
		echo ""; \
		echo "The $(TARGET_PREFIX) toolchain is required for building this OS."; \
		echo ""; \
		echo "Installation directory: $(TOOLCHAIN_DIR)"; \
		echo ""; \
		echo "To build and install the toolchain, run:"; \
		echo ""; \
		echo "  $(COLOR_GREEN)./build_scripts/setup_toolchain.sh$(COLOR_RESET)"; \
		echo ""; \
		echo "Or manually install $(TARGET_PREFIX)-gcc and $(TARGET_PREFIX)-binutils."; \
		echo ""; \
		exit 1; \
	else \
		echo "$(COLOR_GREEN)✓ Toolchain found$(COLOR_RESET)"; \
		echo "  GCC: $(TOOLCHAIN_GCC)"; \
		echo "  Binutils: $(TOOLCHAIN_LD)"; \
		$(CC_CROSS) --version | head -n1; \
		$(LD_CROSS) --version | head -n1; \
	fi

# Setup toolchain automatically
.PHONY: setup-toolchain
setup-toolchain:
	@echo "$(COLOR_BLUE)Setting up cross-compiler toolchain...$(COLOR_RESET)"
	@if [ -x "$(TOOLCHAIN_SETUP_SCRIPT)" ]; then \
		$(TOOLCHAIN_SETUP_SCRIPT); \
	else \
		echo "$(COLOR_RED)Error: Setup script not found or not executable$(COLOR_RESET)"; \
		echo "Expected location: $(TOOLCHAIN_SETUP_SCRIPT)"; \
		exit 1; \
	fi

# Show toolchain information
.PHONY: toolchain-info
toolchain-info:
	@echo "$(COLOR_BLUE)Toolchain Configuration:$(COLOR_RESET)"
	@echo "  Target: $(TARGET_PREFIX)"
	@echo "  Install dir: $(TOOLCHAIN_DIR)"
	@echo "  Setup script: $(TOOLCHAIN_SETUP_SCRIPT)"
	@echo ""
	@if [ -n "$(TOOLCHAIN_GCC)" ]; then \
		echo "$(COLOR_GREEN)Toolchain Status: Installed$(COLOR_RESET)"; \
		echo ""; \
		echo "Available tools:"; \
		echo "  CC:      $(CC_CROSS) -> $(TOOLCHAIN_GCC)"; \
		echo "  CXX:     $(CXX_CROSS)"; \
		echo "  AS:      $(AS_CROSS)"; \
		echo "  LD:      $(LD_CROSS)"; \
		echo "  AR:      $(AR_CROSS)"; \
		echo "  OBJCOPY: $(OBJCOPY)"; \
		echo "  OBJDUMP: $(OBJDUMP)"; \
	else \
		echo "$(COLOR_RED)Toolchain Status: Not installed$(COLOR_RESET)"; \
		echo ""; \
		echo "Run 'make setup-toolchain' to install it."; \
	fi

# Example compiler flags for OS development
CFLAGS_CROSS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra
CXXFLAGS_CROSS = -std=gnu++11 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti
LDFLAGS_CROSS = -nostdlib

# Help target
.PHONY: help-toolchain
help-toolchain:
	@echo "$(COLOR_BLUE)Toolchain Targets:$(COLOR_RESET)"
	@echo "  check-toolchain   - Verify toolchain is installed and working"
	@echo "  setup-toolchain   - Download and build the cross-compiler toolchain"
	@echo "  toolchain-info    - Display toolchain configuration and status"
	@echo ""
	@echo "$(COLOR_BLUE)Toolchain Variables:$(COLOR_RESET)"
	@echo "  TOOLCHAIN_DIR     - Installation directory (default: $(HOME)/opt/cross)"
	@echo "  TARGET_PREFIX     - Target triplet (default: i686-elf)"
	@echo ""
	@echo "$(COLOR_BLUE)Usage Example:$(COLOR_RESET)"
	@echo "  \$$ make setup-toolchain    # First time setup"
	@echo "  \$$ make check-toolchain    # Verify installation"
	@echo "  \$$ make                    # Build your OS"
