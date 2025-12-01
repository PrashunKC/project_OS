#include "vesa.h"
#include "memory.h"
#include "stdio.h"
#include "x86.h"

// Global framebuffer info
static FramebufferInfo fb_info;

// Use a fixed buffer in low memory for VESA BIOS calls
// We'll use 0x7000-0x8000 area (4KB) which should be safe
#define VESA_BUFFER_ADDR 0x7000
#define VESA_INFO_ADDR   0x7000
#define VESA_MODE_ADDR   0x7200

// Helper to convert linear address to segment:offset
static inline uint16_t linear_to_segment(uint32_t addr) {
  return (uint16_t)(addr >> 4);
}

static inline uint16_t linear_to_offset(uint32_t addr) {
  return (uint16_t)(addr & 0x0F);
}

// VESA function: Get VBE Controller Info
int vesa_get_controller_info(VBEInfoBlock *info) {
  // Use the low memory buffer for BIOS call
  VBEInfoBlock *bios_buf = (VBEInfoBlock *)VESA_INFO_ADDR;
  
  // Clear the structure
  memset(bios_buf, 0, sizeof(VBEInfoBlock));

  // Set signature to "VBE2" to request VBE 2.0+ info
  bios_buf->signature[0] = 'V';
  bios_buf->signature[1] = 'B';
  bios_buf->signature[2] = 'E';
  bios_buf->signature[3] = '2';

  // Call VESA BIOS: INT 10h, AX=4F00h
  // ES:DI points to info block
  Registers16 regs;
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x4F00;
  regs.es = linear_to_segment(VESA_INFO_ADDR);
  regs.di = linear_to_offset(VESA_INFO_ADDR);

  x86_Int10(&regs);

  // Check if VESA is supported (AL = 4Fh, AH = 00h for success)
  if ((regs.ax & 0xFFFF) != 0x004F) {
    printf("  VESA call failed: AX=0x%x\n", regs.ax);
    return -1; // VESA not supported
  }

  // Verify signature changed to "VESA"
  if (bios_buf->signature[0] != 'V' || bios_buf->signature[1] != 'E' ||
      bios_buf->signature[2] != 'S' || bios_buf->signature[3] != 'A') {
    printf("  VESA signature mismatch\n");
    return -1;
  }

  // Copy result to caller's buffer
  memcpy(info, bios_buf, sizeof(VBEInfoBlock));
  return 0; // Success
}

// VESA function: Get Mode Info
int vesa_get_mode_info(uint16_t mode, VESAModeInfo *info) {
  // Use the low memory buffer for BIOS call
  VESAModeInfo *bios_buf = (VESAModeInfo *)VESA_MODE_ADDR;
  
  memset(bios_buf, 0, sizeof(VESAModeInfo));

  Registers16 regs;
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x4F01;
  regs.cx = mode;
  regs.es = linear_to_segment(VESA_MODE_ADDR);
  regs.di = linear_to_offset(VESA_MODE_ADDR);

  x86_Int10(&regs);

  if ((regs.ax & 0xFFFF) != 0x004F) {
    return -1;
  }

  // Copy result to caller's buffer
  memcpy(info, bios_buf, sizeof(VESAModeInfo));
  return 0;
}

// VESA function: Set Video Mode
int vesa_set_mode(uint16_t mode) {
  Registers16 regs;
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x4F02;
  regs.bx = mode | 0x4000; // Bit 14: Use linear framebuffer model

  x86_Int10(&regs);

  if ((regs.ax & 0xFFFF) != 0x004F) {
    printf("  Set mode failed: AX=0x%x\n", regs.ax);
    return -1;
  }

  return 0;
}

// Enumerate available VESA modes and find a suitable one
static uint16_t vesa_find_mode(VBEInfoBlock *vbe_info, uint16_t target_width, 
                                uint16_t target_height, uint8_t target_bpp) {
  // Get the mode list pointer (far pointer: segment:offset)
  uint16_t mode_seg = (vbe_info->video_modes_ptr >> 16) & 0xFFFF;
  uint16_t mode_off = vbe_info->video_modes_ptr & 0xFFFF;
  
  // Convert to linear address
  uint32_t mode_list_addr = ((uint32_t)mode_seg << 4) + mode_off;
  uint16_t *mode_list = (uint16_t *)mode_list_addr;
  
  printf("  Mode list at: 0x%x (seg:0x%x off:0x%x)\n", 
         mode_list_addr, mode_seg, mode_off);
  
  VESAModeInfo mode_info;
  uint16_t best_mode = 0;
  int best_score = -1;
  
  // Iterate through mode list (terminated by 0xFFFF)
  for (int i = 0; mode_list[i] != 0xFFFF && i < 256; i++) {
    uint16_t mode = mode_list[i];
    
    if (vesa_get_mode_info(mode, &mode_info) != 0) {
      continue;
    }
    
    // Check if mode supports linear framebuffer and is graphics mode
    if ((mode_info.mode_attributes & 0x90) != 0x90) {
      continue;
    }
    
    // Check for direct color mode (memory_model = 6)
    if (mode_info.memory_model != 6) {
      continue;
    }
    
    // Score based on how close to target resolution
    int score = 0;
    
    // Prefer exact match
    if (mode_info.width == target_width && mode_info.height == target_height 
        && mode_info.bpp == target_bpp) {
      return mode; // Exact match!
    }
    
    // Otherwise score based on closeness
    if (mode_info.bpp >= 24) score += 100; // Prefer high color
    if (mode_info.width >= 800 && mode_info.width <= 1280) score += 50;
    if (mode_info.height >= 600 && mode_info.height <= 1024) score += 50;
    
    if (score > best_score) {
      best_score = score;
      best_mode = mode;
    }
  }
  
  return best_mode;
}

// Initialize VESA and find a suitable mode
int vesa_init(void) {
  VBEInfoBlock vbe_info;

  puts("Initializing VESA...\n");

  // Get VBE controller info
  if (vesa_get_controller_info(&vbe_info) != 0) {
    puts("  VESA not supported!\n");
    return -1;
  }

  printf("  VESA Version: %d.%d\n", vbe_info.version >> 8,
         vbe_info.version & 0xFF);
  printf("  Total memory: %d KB\n", vbe_info.total_memory * 64);

  // Find a suitable mode - try 1024x768x32 first
  uint16_t selected_mode = vesa_find_mode(&vbe_info, 1024, 768, 32);
  
  if (selected_mode == 0) {
    // Fallback: try 800x600x32
    selected_mode = vesa_find_mode(&vbe_info, 800, 600, 32);
  }
  
  if (selected_mode == 0) {
    // Fallback: try 640x480x32
    selected_mode = vesa_find_mode(&vbe_info, 640, 480, 32);
  }

  if (selected_mode == 0) {
    puts("  No suitable VESA mode found!\n");
    return -1;
  }

  // Get final mode info
  VESAModeInfo mode_info;
  if (vesa_get_mode_info(selected_mode, &mode_info) != 0) {
    puts("  Failed to get mode info!\n");
    return -1;
  }

  printf("  Selected mode 0x%x: %dx%dx%d\n", selected_mode,
         mode_info.width, mode_info.height, mode_info.bpp);
  printf("  Framebuffer at: 0x%x\n", mode_info.framebuffer);
  printf("  Pitch: %d bytes/line\n", mode_info.bytes_per_scanline);

  // Set the mode
  if (vesa_set_mode(selected_mode) != 0) {
    puts("  Failed to set VESA mode!\n");
    return -1;
  }

  // Store framebuffer info for kernel
  fb_info.framebuffer_addr = mode_info.framebuffer;
  fb_info.width = mode_info.width;
  fb_info.height = mode_info.height;
  fb_info.pitch = mode_info.bytes_per_scanline;
  fb_info.bpp = mode_info.bpp;
  fb_info.memory_model = mode_info.memory_model;
  
  // Red/Green/Blue field positions
  fb_info.red_mask_size = mode_info.red_mask_size;
  fb_info.red_field_pos = mode_info.red_field_position;
  fb_info.green_mask_size = mode_info.green_mask_size;
  fb_info.green_field_pos = mode_info.green_field_position;
  fb_info.blue_mask_size = mode_info.blue_mask_size;
  fb_info.blue_field_pos = mode_info.blue_field_position;

  puts("  VESA initialized successfully!\n");
  return 0;
}

// Get framebuffer info (to pass to kernel)
FramebufferInfo *vesa_get_framebuffer_info(void) { return &fb_info; }
