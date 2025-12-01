#include "multiboot.h"
#include "stdint.h"

// External functions
extern void kprint(const char *str, uint8_t color);

// Global to store framebuffer info
static struct {
  uint32_t addr;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint8_t bpp;
  uint8_t type;
} framebuffer_info = {0};

void multiboot_init(uint32_t magic, multiboot_info_t *mbi) {
  // Verify magic - silently skip if not multiboot (custom bootloader is fine)
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
    // Not booted by Multiboot - that's OK, we have our own bootloader
    return;
  }

  // Check if framebuffer info is available (bit 12 of flags)
  if (mbi->flags & (1 << 12)) {
    framebuffer_info.addr = (uint32_t)mbi->framebuffer_addr;
    framebuffer_info.width = mbi->framebuffer_width;
    framebuffer_info.height = mbi->framebuffer_height;
    framebuffer_info.pitch = mbi->framebuffer_pitch;
    framebuffer_info.bpp = mbi->framebuffer_bpp;
    framebuffer_info.type = mbi->framebuffer_type;
  }
}

// Get framebuffer address
uint32_t multiboot_get_framebuffer_addr(void) { return framebuffer_info.addr; }

// Get framebuffer width
uint32_t multiboot_get_framebuffer_width(void) {
  return framebuffer_info.width;
}

// Get framebuffer height
uint32_t multiboot_get_framebuffer_height(void) {
  return framebuffer_info.height;
}

// Get framebuffer pitch (bytes per scanline)
uint32_t multiboot_get_framebuffer_pitch(void) {
  return framebuffer_info.pitch;
}

// Get framebuffer bits per pixel
uint8_t multiboot_get_framebuffer_bpp(void) { return framebuffer_info.bpp; }
