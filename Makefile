ASM=nasm
CC=gcc
CC16=wcc
LD16=wlink
SRC_DIR=src
TOOLS_DIR=tools
BUILD_DIR=build
XORRISO ?= $(shell command -v xorriso 2>/dev/null)
GRUB_MKRESCUE ?= $(shell command -v grub-mkrescue 2>/dev/null || command -v grub2-mkrescue 2>/dev/null)
VBOXMANAGE ?= $(shell command -v VBoxManage 2>/dev/null)

.PHONY: all floppy_image bootloader clean always tools_fat stage1 stage2 iso mbr vbr hdd_image iso_noemul grub-iso qemu-test qemu-grub check-vbox-host run-vbox run-vbox-iso run-grub-vbox test diagnostic-test requirements

# Default target
all: floppy_image tools_fat

# Print the dependency matrix without running a build.
requirements:
	@printf '%s\n' \
		'NBOS requirements' \
		'=================' \
		'' \
		'Required for the default build (make):' \
		'  make, gcc, binutils (ld/objcopy), NASM' \
		'  dosfstools (mkfs.fat), mtools (mcopy)' \
		'' \
		'Required for make test diagnostics:' \
		'  Python 3, coreutils timeout, QEMU x86_64' \
		'' \
		'Required for ISO targets:' \
		'  xorriso' \
		'' \
		'Required for GRUB targets:' \
		'  grub-mkrescue (or grub2-mkrescue), grub-pc-bin, xorriso' \
		'' \
		'Required for VirtualBox targets:' \
		'  VirtualBox/VBoxManage; AMD-V/VT-x must not be owned by active KVM' \
		'' \
		'Optional cross-toolchain:' \
		'  i686-elf-gcc and i686-elf-binutils under $$HOME/opt/cross' \
		'  (the current build falls back to host tools when unavailable)' \
		'' \
		'Debian install command:' \
		'  sudo apt install build-essential binutils nasm dosfstools mtools python3 coreutils qemu-system-x86 xorriso grub-pc-bin virtualbox'

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

$(BUILD_DIR)/main.iso: $(BUILD_DIR)/main_floppy.img sdk
	@if [ -z "$(XORRISO)" ]; then \
		echo "Error: xorriso is required for ISO targets. Install it with: sudo apt-get install xorriso"; \
		exit 1; \
	fi
	mkdir -p $(BUILD_DIR)/iso
	cp $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/iso/
	cp -r sdk $(BUILD_DIR)/iso/
	$(XORRISO) -as mkisofs -b main_floppy.img \
		-c boot.cat \
		-o $(BUILD_DIR)/main.iso \
		$(BUILD_DIR)/iso
	rm -rf $(BUILD_DIR)/iso

#
#	SDK (Software Development Kit)
#
sdk: $(BUILD_DIR)/sdk

$(BUILD_DIR)/sdk: always
	mkdir -p $(BUILD_DIR)/sdk
	cp -r sdk/* $(BUILD_DIR)/sdk/
	# Build CRT0
	nasm -f elf64 -o $(BUILD_DIR)/sdk/lib/crt0.o sdk/lib/crt0.asm
	@echo "SDK built successfully"

#
#	GRUB ISO image (for Multiboot graphics)
#
grub-iso: kernel
	@if [ -z "$(GRUB_MKRESCUE)" ]; then \
		echo "Error: grub-mkrescue or grub2-mkrescue is required for GRUB ISO targets."; \
		echo "Install GRUB rescue tools and xorriso before retrying."; \
		exit 1; \
	fi
	@if [ -z "$(XORRISO)" ]; then \
		echo "Error: xorriso is required for GRUB ISO targets. Install it with: sudo apt-get install xorriso"; \
		exit 1; \
	fi
	@if [ ! -d /usr/lib/grub/i386-pc ]; then \
		echo "Error: BIOS GRUB modules are missing. Install them with: sudo apt-get install grub-pc-bin"; \
		exit 1; \
	fi
	mkdir -p iso/boot/grub
	@printf '%s\n' \
		'set timeout=0' \
		'set default=0' \
		'set gfxpayload=text' \
		'menuentry "NBOS" {' \
		'    multiboot /boot/kernel.elf' \
		'    boot' \
		'}' > iso/boot/grub/grub.cfg
	cp $(BUILD_DIR)/kernel.elf iso/boot/kernel.elf
	$(GRUB_MKRESCUE) -o $(BUILD_DIR)/project_os.iso iso/

#
#	ISO image using the known-good floppy emulation path
#
iso_noemul: $(BUILD_DIR)/main_noemul.iso

$(BUILD_DIR)/main_noemul.iso: $(BUILD_DIR)/main_floppy.img sdk
	@if [ -z "$(XORRISO)" ]; then \
		echo "Error: xorriso is required for ISO targets. Install it with: sudo apt-get install xorriso"; \
		exit 1; \
	fi
	mkdir -p $(BUILD_DIR)/iso_noemul
	cp $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/iso_noemul/
	cp -r sdk $(BUILD_DIR)/iso_noemul/
	$(XORRISO) -as mkisofs \
		-b main_floppy.img \
		-c boot.cat \
		-o $(BUILD_DIR)/main_noemul.iso \
		$(BUILD_DIR)/iso_noemul
	rm -rf $(BUILD_DIR)/iso_noemul

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
	printf '\200\001\000\000\006\040\020\000\000\010\000\000\000\370\000\000' | dd of=$(BUILD_DIR)/main_hdd.img bs=1 seek=446 conv=notrunc
	# Write boot signature
	printf '\125\252' | dd of=$(BUILD_DIR)/main_hdd.img bs=1 seek=510 conv=notrunc
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

check-vbox-host:
	@if [ -z "$(VBOXMANAGE)" ]; then \
		echo "Error: VBoxManage is required for VirtualBox targets. Install VirtualBox first."; \
		exit 1; \
	fi
	@if [ -e /dev/kvm ] && grep -qE '^(kvm|kvm_amd|kvm_intel) ' /proc/modules; then \
		echo "KVM is loaded; attempting to release the CPU virtualization extension for VirtualBox..."; \
		modules="$$(awk '$$1 == "kvm_amd" || $$1 == "kvm_intel" || $$1 == "kvm" { print $$1 }' /proc/modules)"; \
		if [ "$$(id -u)" -eq 0 ]; then \
			modprobe -r $$modules; \
		elif command -v sudo >/dev/null 2>&1; then \
			sudo modprobe -r $$modules; \
		else \
			echo "Error: sudo is required to unload KVM modules."; \
			exit 1; \
		fi; \
		if grep -qE '^(kvm|kvm_amd|kvm_intel) ' /proc/modules; then \
			echo "Error: KVM is still in use; VirtualBox cannot start (VERR_SVM_IN_USE/VERR_VMX_IN_USE)."; \
			echo "Inspect the owner with: sudo fuser -v /dev/kvm"; \
			exit 1; \
		fi; \
		echo "KVM modules unloaded successfully."; \
	fi

run-vbox: check-vbox-host $(BUILD_DIR)/main_floppy.img
	@echo "Setting up VirtualBox VM..."
	@for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do \
		if ! VBoxManage showvminfo "$(VBOX_VM_NAME)" > /dev/null 2>&1; then break; fi; \
		VBoxManage controlvm "$(VBOX_VM_NAME)" poweroff > /dev/null 2>&1 || true; \
		sleep 1; \
		VBoxManage unregistervm "$(VBOX_VM_NAME)" --delete > /dev/null 2>&1 && break; \
	done
	@if VBoxManage showvminfo "$(VBOX_VM_NAME)" > /dev/null 2>&1; then \
		echo "Error: could not remove the existing $(VBOX_VM_NAME) VM."; \
		exit 1; \
	fi
	@VBoxManage createvm --name "$(VBOX_VM_NAME)" --ostype Other --register
	@VBoxManage modifyvm "$(VBOX_VM_NAME)" --memory 128 --vram 16 --graphicscontroller vga --hwvirtex off --longmode on
	@VBoxManage storagectl "$(VBOX_VM_NAME)" --name "Floppy" --add floppy
	@VBoxManage storageattach "$(VBOX_VM_NAME)" --storagectl "Floppy" --port 0 --device 0 --type fdd --medium "$(abspath $(BUILD_DIR)/main_floppy.img)"
	@echo "Starting VirtualBox..."
	@VBoxManage startvm "$(VBOX_VM_NAME)"

run-vbox-iso: check-vbox-host iso
	@echo "Setting up VirtualBox VM for ISO boot..."
	@for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do \
		if ! VBoxManage showvminfo "NBOS_ISO" > /dev/null 2>&1; then break; fi; \
		VBoxManage controlvm "NBOS_ISO" poweroff > /dev/null 2>&1 || true; \
		sleep 1; \
		VBoxManage unregistervm "NBOS_ISO" --delete > /dev/null 2>&1 && break; \
	done
	@if VBoxManage showvminfo "NBOS_ISO" > /dev/null 2>&1; then \
		echo "Error: could not remove the existing NBOS_ISO VM."; \
		exit 1; \
	fi
	VBoxManage createvm --name "NBOS_ISO" --ostype "Other_64" --register
	VBoxManage modifyvm "NBOS_ISO" --memory 128 --vram 64 --graphicscontroller vboxvga --cpus 1 --hwvirtex on --longmode on
	VBoxManage storagectl "NBOS_ISO" --name "IDE" --add ide
	VBoxManage storageattach "NBOS_ISO" --storagectl "IDE" --port 0 --device 0 --type dvddrive --medium $(abspath $(BUILD_DIR)/main.iso)
	@echo "Starting VirtualBox..."
	VBoxManage startvm "NBOS_ISO"

# Run GRUB ISO in VirtualBox
run-grub-vbox: check-vbox-host grub-iso
	@echo "Setting up VirtualBox VM for GRUB ISO..."
	@for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do \
		if ! VBoxManage showvminfo "NBOS_GRUB" > /dev/null 2>&1; then break; fi; \
		VBoxManage controlvm "NBOS_GRUB" poweroff > /dev/null 2>&1 || true; \
		sleep 1; \
		VBoxManage unregistervm "NBOS_GRUB" --delete > /dev/null 2>&1 && break; \
	done
	@if VBoxManage showvminfo "NBOS_GRUB" > /dev/null 2>&1; then \
		echo "Error: could not remove the existing NBOS_GRUB VM."; \
		exit 1; \
	fi
	VBoxManage createvm --name "NBOS_GRUB" --ostype "Other_64" --register
	VBoxManage modifyvm "NBOS_GRUB" --memory 128 --vram 16 --cpus 1 --graphicscontroller vboxvga --hwvirtex on --longmode on
	VBoxManage storagectl "NBOS_GRUB" --name "IDE" --add ide
	VBoxManage storageattach "NBOS_GRUB" --storagectl "IDE" --port 0 --device 0 --type dvddrive --medium $(abspath $(BUILD_DIR)/project_os.iso)
	@echo "Starting VirtualBox..."
	VBoxManage startvm "NBOS_GRUB"

# Test the custom boot path with QEMU. The flat kernel image is not a Linux
# bzImage, so it must be booted through the FAT floppy loader.
qemu-test: floppy_image
	qemu-system-x86_64 -drive format=raw,if=floppy,file=$(BUILD_DIR)/main_floppy.img -serial stdio

# Test GRUB ISO with QEMU
qemu-grub: grub-iso
	qemu-system-x86_64 -cdrom $(BUILD_DIR)/project_os.iso -serial stdio

# Full non-interactive build, image, and emulator diagnostics.
test diagnostic-test:
	./test.sh
