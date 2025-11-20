BUILD_DIR?=build
ASM?=nasm

.PHONY: all
all: stage2

# Include toolchain definitions
include ../../../build_scripts/toolchain.mk

TARGET_ASMFLAGS += -f elf
TARGET_CFLAGS += -ffreestanding -O2 -Wall -Wextra -I. -std=c99
TARGET_LIBS += -lgcc
TARGET_LINKFLAGS += -T linker.ld -nostdlib

SOURCES_C=$(wildcard *.c)
SOURCES_ASM=$(wildcard *.asm)
OBJECTS_C=$(patsubst %.c,$(BUILD_DIR)/stage2/c/%.o, $(SOURCES_C))
OBJECTS_ASM=$(patsubst %.asm,$(BUILD_DIR)/stage2/asm/%.o, $(SOURCES_ASM))

.PHONY: all stage2 clean

all: stage2

stage2: $(BUILD_DIR)/stage2.bin

$(BUILD_DIR)/stage2.bin: $(OBJECTS_ASM) $(OBJECTS_C)
	$(LD_CROSS) $(TARGET_LINKFLAGS) -o $@ $(OBJECTS_ASM) $(OBJECTS_C)

$(BUILD_DIR)/stage2/c/%.o: %.c always
	$(CC_CROSS) $(TARGET_CFLAGS) -c -o $@ $<

$(BUILD_DIR)/stage2/asm/%.o: %.asm always
	$(ASM) $(TARGET_ASMFLAGS) -o $@ $<

always:
	mkdir -p $(BUILD_DIR)/stage2/c
	mkdir -p $(BUILD_DIR)/stage2/asm

clean:
	rm -f $(BUILD_DIR)/stage2.bin
