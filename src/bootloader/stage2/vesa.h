#pragma once
#include "stdint.h"

// VESA VBE Structures


// VBE Info Block (INT 10h, AX=4F00h)
typedef struct 
{
  char signature[4];        // "VESA"
  uint16_t version;         // VBE version (0x0300 for VBE 3.0)
  uint32_t oem_string_ptr;  // Far pointer to OEM string
  uint32_t capabilities;    // Capabilities flags
  uint32_t video_modes_ptr; // Far pointer to video mode list
  uint16_t total_memory;    // Total memory in 64KB blocks
  // VBE 2.0+
  uint16_t oem_software_rev;
  uint32_t oem_vendor_name_ptr;
  uint32_t oem_product_name_ptr;
  uint32_t oem_product_rev_ptr;
  uint8_t reserved[222];
  uint8_t oem_data[256];
} __attribute__((packed)) VBEInfoBlock;


// VESA Mode Info Block (INT 10h, AX=4F01h)
typedef struct 
{
  // Mandatory info for all VBE revisions
  uint16_t mode_attributes;
  uint8_t window_a_attributes;
  uint8_t window_b_attributes;
  uint16_t window_granularity;
  uint16_t window_size;
  uint16_t window_a_segment;
  uint16_t window_b_segment;
  uint32_t window_function_ptr;
  uint16_t bytes_per_scanline;

  // VBE 1.2+
  uint16_t width;
  uint16_t height;
  uint8_t char_width;
  uint8_t char_height;
  uint8_t planes;
  uint8_t bpp;
  uint8_t banks;
  uint8_t memory_model;
  uint8_t bank_size;
  uint8_t image_pages;
  uint8_t reserved1;

  // Direct Color fields
  uint8_t red_mask_size;
  uint8_t red_field_position;
  uint8_t green_mask_size;
  uint8_t green_field_position;
  uint8_t blue_mask_size;
  uint8_t blue_field_position;
  uint8_t reserved_mask_size;
  uint8_t reserved_field_position;
  uint8_t direct_color_attributes;

  // VBE 2.0+
  uint32_t framebuffer;
  uint32_t off_screen_mem_offset;
  uint16_t off_screen_mem_size;
  uint8_t reserved2[206];
} __attribute__((packed)) VESAModeInfo;


// Framebuffer info to pass to kernel
typedef struct 
{
  uint32_t framebuffer_addr;
  uint32_t width;
  uint32_t height;
  uint32_t pitch; // Bytes per scanline
  uint8_t bpp;    // Bits per pixel
  uint8_t memory_model;
  uint8_t red_mask_size;
  uint8_t red_field_pos;
  uint8_t green_mask_size;
  uint8_t green_field_pos;
  uint8_t blue_mask_size;
  uint8_t blue_field_pos;
} __attribute__((packed)) FramebufferInfo;


// Function prototypes
int vesa_init(void);
int vesa_set_mode(uint16_t mode);
FramebufferInfo *vesa_get_framebuffer_info(void);
