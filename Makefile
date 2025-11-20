ASM=nasm
CC=gcc
CC16=wcc
LD16=wlink
SRC_DIR=src
TOOLS_DIR=tools
BUILD_DIR=build

.PHONY: all floppy_image bootloader clean always tools_fat stage1 stage2

# Default target
all: floppy_image tools_fat

# Stage2
stage2: $(BUILD_DIR)/stage2.bin

$(BUILD_DIR)/stage2.bin: always
	$(MAKE) -C $(SRC_DIR)/bootloader/stage2 BUILD_DIR=$(abspath $(BUILD_DIR))
# Kernel
kernel: $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: always
	$(MAKE) -C $(SRC_DIR)/kernel BUILD_DIR=$(abspath $(BUILD_DIR))

#
#	Floppy image
#
floppy_image: $(BUILD_DIR)/main_floppy.img

$(BUILD_DIR)/main_floppy.img: bootloader stage2 kernel
	dd if=/dev/zero of=$(BUILD_DIR)/main_floppy.img bs=512 count=2880
	mkfs.fat -F 12  -n "NBOS" $(BUILD_DIR)/main_floppy.img
	dd if=$(BUILD_DIR)/stage1.bin of=$(BUILD_DIR)/main_floppy.img conv=notrunc
	mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/stage2.bin "::/stage2.bin"
	mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/kernel.bin "::/kernel.bin"
	# mcopy -i $(BUILD_DIR)/main_floppy.img test.txt "::/test.txt"

#
#	Bootloader
#
bootloader: stage1 stage2

stage1: $(BUILD_DIR)/stage1.bin

$(BUILD_DIR)/stage1.bin: always
	@if [ -f "$(SRC_DIR)/bootloader/stage1/Makefile" ]; then \
		$(MAKE) -C $(SRC_DIR)/bootloader/stage1 BUILD_DIR=$(abspath $(BUILD_DIR)); \
	else \
		$(ASM) $(SRC_DIR)/bootloader/stage1/boot.asm -f bin -o $(BUILD_DIR)/stage1.bin; \
	fi



#
#	Always (utility target to ensure build dir exists)
#
always:
	mkdir -p $(BUILD_DIR)

#
#	Clean
#
clean:
	@if [ -f "$(SRC_DIR)/bootloader/stage1/Makefile" ]; then \
		$(MAKE) -C $(SRC_DIR)/bootloader/stage1 BUILD_DIR=$(abspath $(BUILD_DIR)) clean; \
	fi
	@if [ -f "$(SRC_DIR)/bootloader/stage2/Makefile" ]; then \
		$(MAKE) -C $(SRC_DIR)/bootloader/stage2 BUILD_DIR=$(abspath $(BUILD_DIR)) clean; \
	fi
	rm -f $(SRC_DIR)/bootloader/stage2/*.err
	rm -f $(SRC_DIR)/bootloader/stage2/fat.err
	rm -rf $(BUILD_DIR)/*