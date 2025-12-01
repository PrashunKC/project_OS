BUILD_DIR?=build
ASM?=nasm

.PHONY: all
all: kernel

# Include toolchain definitions (relative to project root)
include ../../build_scripts/toolchain.mk

# 64-bit kernel build configuration
CC_CROSS := gcc
LD_CROSS := ld

# 64-bit compilation flags
TARGET_CFLAGS += -ffreestanding -O2 -Wall -Wextra -std=c99 -m64 -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-pie -fno-pic
TARGET_LINKFLAGS += -T linker.ld -nostdlib -m elf_x86_64 -no-pie

SOURCES_C=$(filter-out idt64.c isr64.c, $(wildcard *.c))
SOURCES_ASM=entry.asm interrupts.asm
OBJECTS_C=$(patsubst %.c,$(BUILD_DIR)/kernel/c/%.o, $(SOURCES_C))
OBJECTS_ASM=$(patsubst %.asm,$(BUILD_DIR)/kernel/asm/%.o, $(SOURCES_ASM))

.PHONY: all kernel clean

all: kernel

kernel: $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	objcopy -O binary $< $@

$(BUILD_DIR)/kernel.elf: $(OBJECTS_ASM) $(OBJECTS_C)
	$(LD_CROSS) $(TARGET_LINKFLAGS) -o $@ $(OBJECTS_ASM) $(OBJECTS_C)

$(BUILD_DIR)/kernel/c/%.o: %.c always
	$(CC_CROSS) $(TARGET_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/kernel/asm/%.o: %.asm always
	$(ASM) -f elf64 -o $@ $<

always:
	mkdir -p $(BUILD_DIR)/kernel/c
	mkdir -p $(BUILD_DIR)/kernel/asm

clean:
	rm -f $(BUILD_DIR)/kernel.bin
