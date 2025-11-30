ASM=nasm
CC=gcc
CC16=wcc
LD16=wlink
SRC_DIR=src
TOOLS_DIR=tools
BUILD_DIR=build

.PHONY: all floppy_image bootloader clean always tools_fat stage1 stage2 iso mbr vbr hdd_image iso_noemul run-vbox run-vbox-iso

# Default target
all: floppy_image tools_fat

# Include toolchain configuration
include build_scripts/toolchain.mk

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
#	ISO image
#
iso: $(BUILD_DIR)/main.iso

$(BUILD_DIR)/main.iso: $(BUILD_DIR)/main_floppy.img
	mkdir -p $(BUILD_DIR)/iso
	cp $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/iso/
	xorriso -as mkisofs -b main_floppy.img \
		-o $(BUILD_DIR)/main.iso \
		$(BUILD_DIR)/iso

#
#	ISO image (no emulation mode)
#
iso_noemul: $(BUILD_DIR)/main_noemul.iso

$(BUILD_DIR)/main_noemul.iso: vbr stage2 kernel
	mkdir -p $(BUILD_DIR)/iso_noemul
	cp $(BUILD_DIR)/vbr.bin $(BUILD_DIR)/iso_noemul/boot.bin
	cp $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/iso_noemul/
	cp $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/iso_noemul/
	xorriso -as mkisofs \
		-b boot.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-o $(BUILD_DIR)/main_noemul.iso \
		$(BUILD_DIR)/iso_noemul

#
#	Hard disk image
#
hdd_image: $(BUILD_DIR)/main_hdd.img

$(BUILD_DIR)/main_hdd.img: mbr vbr stage2 kernel
	# Create 32MB hard disk image
	dd if=/dev/zero of=$(BUILD_DIR)/main_hdd.img bs=1M count=32
	# Write MBR
	dd if=$(BUILD_DIR)/mbr.bin of=$(BUILD_DIR)/main_hdd.img conv=notrunc bs=446 count=1
	# Create partition table (one FAT16 partition, type 0x06, starting at sector 2048)
	printf '\\x80\\x00\\x01\\x00\\x06\\x00\\x20\\x10\\x00\\x08\\x00\\x00\\x00\\xF8\\x00\\x00' | dd of=$(BUILD_DIR)/main_hdd.img bs=1 seek=446 conv=notrunc
	# Write boot signature
	printf '\\x55\\xAA' | dd of=$(BUILD_DIR)/main_hdd.img bs=1 seek=510 conv=notrunc
	# Format partition as FAT16
	mkfs.fat -F 16 -n "NBOS" --offset 2048 $(BUILD_DIR)/main_hdd.img
	# Write VBR to partition start
	dd if=$(BUILD_DIR)/vbr.bin of=$(BUILD_DIR)/main_hdd.img conv=notrunc bs=512 seek=2048
	# Copy files to partition
	mcopy -i $(BUILD_DIR)/main_hdd.img@@1M $(BUILD_DIR)/stage2.bin "::/stage2.bin"
	mcopy -i $(BUILD_DIR)/main_hdd.img@@1M $(BUILD_DIR)/kernel.bin "::/kernel.bin"


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
#	MBR (Master Boot Record)
#
mbr: $(BUILD_DIR)/mbr.bin

$(BUILD_DIR)/mbr.bin: always
	$(ASM) $(SRC_DIR)/bootloader/mbr/mbr.asm -f bin -o $(BUILD_DIR)/mbr.bin

#
#	VBR (Volume Boot Record) - enhanced version with LBA support
#
vbr: $(BUILD_DIR)/vbr.bin

$(BUILD_DIR)/vbr.bin: always
	$(ASM) $(SRC_DIR)/bootloader/vbr/vbr.asm -f bin -o $(BUILD_DIR)/vbr.bin


#
#	Tools
#
tools_fat: $(BUILD_DIR)/tools/fat

$(BUILD_DIR)/tools/fat: always $(TOOLS_DIR)/fat/fat.c
	mkdir -p $(BUILD_DIR)/tools
	$(CC) -g -o $(BUILD_DIR)/tools/fat $(TOOLS_DIR)/fat/fat.c

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
	rm -rf $(BUILD_DIR)/stage1.bin				
	
#
#	The last line "rm -rf $(BUILD_DIR)/stage1.bin" is there to prevent the file from somehow coming back.... It seemed like it always comes back.... Hopefully this fixes that....
#

#
#	VirtualBox targets
#
VBOX_VM_NAME := NBOS

run-vbox: $(BUILD_DIR)/main_floppy.img
	@echo "Setting up VirtualBox VM..."
	@VBoxManage showvminfo "$(VBOX_VM_NAME)" > /dev/null 2>&1 && VBoxManage unregistervm "$(VBOX_VM_NAME)" --delete || true
	@VBoxManage createvm --name "$(VBOX_VM_NAME)" --ostype Other --register
	@VBoxManage modifyvm "$(VBOX_VM_NAME)" --memory 32 --vram 16 --graphicscontroller vboxvga --hwvirtex off
	@VBoxManage storagectl "$(VBOX_VM_NAME)" --name "Floppy" --add floppy
	@VBoxManage storageattach "$(VBOX_VM_NAME)" --storagectl "Floppy" --port 0 --device 0 --type fdd --medium "$(abspath $(BUILD_DIR)/main_floppy.img)"
	@echo "Starting VirtualBox..."
	@VBoxManage startvm "$(VBOX_VM_NAME)"

run-vbox-iso: $(BUILD_DIR)/main.iso
	@echo "Setting up VirtualBox VM (ISO)..."
	@VBoxManage showvminfo "$(VBOX_VM_NAME)" > /dev/null 2>&1 && VBoxManage unregistervm "$(VBOX_VM_NAME)" --delete || true
	@VBoxManage createvm --name "$(VBOX_VM_NAME)" --ostype Other --register
	@VBoxManage modifyvm "$(VBOX_VM_NAME)" --memory 32 --vram 16 --graphicscontroller vboxvga --hwvirtex off
	@VBoxManage storagectl "$(VBOX_VM_NAME)" --name "IDE" --add ide
	@VBoxManage storageattach "$(VBOX_VM_NAME)" --storagectl "IDE" --port 0 --device 0 --type dvddrive --medium "$(abspath $(BUILD_DIR)/main.iso)"
	@echo "Starting VirtualBox..."
	@VBoxManage startvm "$(VBOX_VM_NAME)"
