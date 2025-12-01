#include "disk.h"
#include "fat.h"
#include "memory.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "vesa.h"

#define _cdecl __attribute__((cdecl))

// Log level definitions for enhanced debugging
#define LOG_INFO "[INFO] "
#define LOG_OK "[OK]   "
#define LOG_ERROR "[ERR]  "
#define LOG_DEBUG "[DBG]  "

// Fixed address where we store framebuffer info for kernel
#define FB_INFO_ADDR 0x8000

// VESA info locations (set by assembly in real mode)
#define VESA_AVAILABLE_ADDR 0x80FF
#define VESA_MODE_INFO_ADDR 0x8100

void _cdecl cstart_(uint16_t bootDrive) {
  printf("\r\n\r\n");
  printf("**************************************************\r\n");
  printf("* BOOTLOADER STAGE 2 STARTED - GCC BUILD        *\r\n");
  printf("* Version: 1.0.0 | Date: 2025-11-30             *\r\n");
  printf("**************************************************\r\n");
  printf("\r\n");

  printf(LOG_INFO "Boot drive: 0x%x\r\n", bootDrive);
  printf(LOG_INFO "Initializing system components...\r\n");
  printf("\r\n");

  // Step 1: Initialize disk subsystem
  printf(LOG_INFO "Step 1: Initializing disk subsystem...\r\n");
  DISK disk;
  if (!DISK_Initialize(&disk, (uint8_t)bootDrive)) {
    printf(LOG_ERROR "Could not initialize disk!\r\n");
    printf(LOG_ERROR "Boot halted. Please check disk connection.\r\n");
    goto end;
  }
  printf(LOG_OK "Disk initialized successfully\r\n");
  printf(LOG_DEBUG "Disk ID: 0x%x, Cylinders: %u, Heads: %u, Sectors: %u\r\n",
         disk.id, disk.cylinders, disk.heads, disk.sectors);
  printf("\r\n");

  // Step 2: Initialize FAT filesystem
  printf(LOG_INFO "Step 2: Initializing FAT filesystem...\r\n");
  if (!FAT_Initialize(&disk)) {
    printf(LOG_ERROR "Could not initialize FAT filesystem!\r\n");
    printf(LOG_ERROR "Boot halted. Please verify disk format.\r\n");
    goto end;
  }
  printf(LOG_OK "FAT filesystem initialized successfully\r\n");
  printf("\r\n");

  // Step 3: Load Kernel
  printf(LOG_INFO "Step 3: Loading kernel...\r\n");
  printf(LOG_INFO "Searching for kernel.bin in root directory...\r\n");
  FAT_File *fd = FAT_Open(&disk, "/kernel.bin");
  if (fd == NULL) {
    printf(LOG_ERROR "Could not open kernel.bin!\r\n");
    printf(LOG_ERROR "Boot halted. Kernel file not found.\r\n");
    goto end;
  }
  printf(LOG_OK "Kernel file found\r\n");
  printf(LOG_DEBUG "Kernel size: %u bytes\r\n", fd->Size);
  printf("\r\n");

  // Step 4: Read kernel to temporary buffer
  printf(LOG_INFO
         "Step 4: Reading kernel to temporary buffer (0x30000)...\r\n");
  uint8_t *kernelBuffer = (uint8_t *)0x30000;
  uint32_t read = FAT_Read(&disk, fd, fd->Size, kernelBuffer);

  if (read != fd->Size) {
    printf(LOG_ERROR "Could not read entire kernel!\r\n");
    printf(LOG_ERROR "Expected: %u bytes, Read: %u bytes\r\n", fd->Size, read);
    printf(LOG_ERROR "Boot halted. Disk read error.\r\n");
    goto end;
  }
  printf(LOG_OK "Kernel read successfully (%u bytes)\r\n", read);

  FAT_Close(fd);
  printf("\r\n");

  // Step 5: Copy kernel to final location
  printf(LOG_INFO "Step 5: Copying kernel to 0x100000 (1MB mark)...\r\n");
  uint8_t *kernelStart = (uint8_t *)0x100000;
  memcpy(kernelStart, kernelBuffer, fd->Size);
  printf(LOG_OK "Kernel copied to 0x100000\r\n");
  printf("\r\n");

  // Step 6: Check VESA graphics (set up in real mode by assembly)
  printf(LOG_INFO "Step 6: Checking VESA graphics...\r\n");
  
  // Copy framebuffer info to fixed address for kernel
  FramebufferInfo *fb_dest = (FramebufferInfo *)FB_INFO_ADDR;
  
  // Read VESA availability flag from fixed location
  uint8_t vesa_available = *(volatile uint8_t *)VESA_AVAILABLE_ADDR;
  
  if (vesa_available) {
    // Parse mode info from fixed memory location
    uint8_t *mode_info = (uint8_t *)VESA_MODE_INFO_ADDR;
    
    // VESAModeInfo structure offsets:
    // 16: bytes_per_scanline (word)
    // 18: width (word), 20: height (word), 25: bpp (byte)
    // 27: memory_model (byte)
    // 31-36: color field info
    // 40: framebuffer (dword)
    fb_dest->width = *(uint16_t *)(mode_info + 18);
    fb_dest->height = *(uint16_t *)(mode_info + 20);
    fb_dest->bpp = mode_info[25];
    fb_dest->pitch = *(uint16_t *)(mode_info + 16);
    fb_dest->framebuffer_addr = *(uint32_t *)(mode_info + 40);
    fb_dest->memory_model = mode_info[27];
    
    // Color field info
    fb_dest->red_mask_size = mode_info[31];
    fb_dest->red_field_pos = mode_info[32];
    fb_dest->green_mask_size = mode_info[33];
    fb_dest->green_field_pos = mode_info[34];
    fb_dest->blue_mask_size = mode_info[35];
    fb_dest->blue_field_pos = mode_info[36];
    
    printf(LOG_OK "VESA mode set: %ux%ux%u\r\n", 
           fb_dest->width, fb_dest->height, fb_dest->bpp);
    printf(LOG_DEBUG "Framebuffer at: 0x%x, pitch: %u\r\n",
           fb_dest->framebuffer_addr, fb_dest->pitch);
  } else {
    // Clear framebuffer info to indicate text mode
    memset(fb_dest, 0, sizeof(FramebufferInfo));
    printf(LOG_INFO "VESA not available, continuing in text mode\r\n");
  }
  printf("\r\n");

  // Step 7: Transfer control to kernel
  printf(LOG_INFO "Step 7: Transferring control to kernel...\r\n");
  printf(LOG_INFO "Jumping to kernel entry point at 0x100000\r\n");
  printf("**************************************************\r\n");
  printf("*           KERNEL HANDOFF IN PROGRESS          *\r\n");
  printf("**************************************************\r\n");
  printf("\r\n");

  // Create a minimal Multiboot info structure (zeroed out for now)
  static uint32_t multiboot_info[32] = {0};

  // Use inline assembly with explicit immediate moves
  // This bypasses any compiler register allocation issues
  // Jump to _start which is 16 bytes after kernel start (past multiboot header + alignment)
  uint32_t entryPoint = (uint32_t)kernelStart + 16;
  __asm__ volatile("movl $0x2BADB002, %%eax\n" // Multiboot magic
                   "movl %0, %%ebx\n"          // Multiboot info
                   "jmp *%1\n"                 // Jump to kernel entry
                   :
                   : "r"((uint32_t)multiboot_info), "r"(entryPoint)
                   : "eax", "ebx");

  // Should never reach here
  for (;;)
    ;

end:
  printf("\r\n");
  printf("**************************************************\r\n");
  printf("*              SYSTEM HALTED                    *\r\n");
  printf("**************************************************\r\n");
  for (;;)
    ;
}
