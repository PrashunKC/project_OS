# NBOS SDK Common Makefile Include
# Include this in your project Makefiles

NBOS_SDK ?= $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))/..
NBOS_INCLUDE = $(NBOS_SDK)/include
NBOS_LIB = $(NBOS_SDK)/lib

# Compiler selection
ifneq ($(shell which x86_64-elf-gcc 2>/dev/null),)
    CC = x86_64-elf-gcc
    LD = x86_64-elf-ld
    AS = nasm
else
    CC = gcc
    LD = ld
    AS = nasm
endif

# Compiler flags
CFLAGS = -ffreestanding -O2 -Wall -Wextra -std=c99
CFLAGS += -m64 -mcmodel=large -mno-red-zone
CFLAGS += -mno-mmx -mno-sse -mno-sse2
CFLAGS += -fno-stack-protector -fno-pic
CFLAGS += -I$(NBOS_INCLUDE)

# Assembler flags
ASFLAGS = -f elf64

# Linker flags  
LDFLAGS = -nostdlib -static -m elf_x86_64

# CRT object
CRT0 = $(NBOS_LIB)/crt0.o

# Build CRT if needed
$(CRT0): $(NBOS_LIB)/crt0.asm
	$(AS) $(ASFLAGS) -o $@ $<

# Wrapper command
NBOS_CC = $(NBOS_SDK)/tools/nbos-gcc
