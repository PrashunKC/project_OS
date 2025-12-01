#pragma once
#include "stdint.h"

// Multiboot header magic numbers
#define MULTIBOOT_HEADER_MAGIC 0x1BADB002
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

// Multiboot header flags
#define MULTIBOOT_PAGE_ALIGN 0x00000001
#define MULTIBOOT_MEMORY_INFO 0x00000002
#define MULTIBOOT_VIDEO_MODE 0x00000004

// Multiboot info structure (passed by GRUB in EBX)
typedef struct {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  uint32_t cmdline;
  uint32_t mods_count;
  uint32_t mods_addr;
  uint32_t syms[4];
  uint32_t mmap_length;
  uint32_t mmap_addr;
  uint32_t drives_length;
  uint32_t drives_addr;
  uint32_t config_table;
  uint32_t boot_loader_name;
  uint32_t apm_table;
  uint32_t vbe_control_info;
  uint32_t vbe_mode_info;
  uint16_t vbe_mode;
  uint16_t vbe_interface_seg;
  uint16_t vbe_interface_off;
  uint16_t vbe_interface_len;

  // Framebuffer info (if flags bit 12 is set)
  uint64_t framebuffer_addr;
  uint32_t framebuffer_pitch;
  uint32_t framebuffer_width;
  uint32_t framebuffer_height;
  uint8_t framebuffer_bpp;
  uint8_t framebuffer_type;
  uint8_t color_info[6];
} __attribute__((packed)) multiboot_info_t;

// Function to parse Multiboot info
void multiboot_init(uint32_t magic, multiboot_info_t *mbi);

// Framebuffer getters
uint32_t multiboot_get_framebuffer_addr(void);
uint32_t multiboot_get_framebuffer_width(void);
uint32_t multiboot_get_framebuffer_height(void);
uint32_t multiboot_get_framebuffer_pitch(void);
uint8_t multiboot_get_framebuffer_bpp(void);
