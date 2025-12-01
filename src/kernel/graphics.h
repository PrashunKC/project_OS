#pragma once
#include "stdint.h"

// Framebuffer info structure (matches bootloader's FramebufferInfo)
typedef struct {
  uint32_t framebuffer_addr;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint8_t bpp;
  uint8_t memory_model;
  uint8_t red_mask_size;
  uint8_t red_field_pos;
  uint8_t green_mask_size;
  uint8_t green_field_pos;
  uint8_t blue_mask_size;
  uint8_t blue_field_pos;
} __attribute__((packed)) FramebufferInfo;

// Initialize graphics system from bootloader's framebuffer info
void graphics_init(void);

// Check if we're in graphics mode
int graphics_is_available(void);

// Get framebuffer info
FramebufferInfo *graphics_get_info(void);

// Basic drawing primitives
void graphics_clear(uint32_t color);
void graphics_put_pixel(int x, int y, uint32_t color);
uint32_t graphics_get_pixel(int x, int y);
void graphics_draw_rect(int x, int y, int w, int h, uint32_t color);
void graphics_fill_rect(int x, int y, int w, int h, uint32_t color);
void graphics_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void graphics_draw_circle(int cx, int cy, int radius, uint32_t color);
void graphics_fill_circle(int cx, int cy, int radius, uint32_t color);

// Text rendering (simple built-in font)
void graphics_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void graphics_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg);
int graphics_get_font_width(void);
int graphics_get_font_height(void);

// Color helpers
#define RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define RGBA(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

// Common colors
#define COLOR_BLACK       RGB(0, 0, 0)
#define COLOR_WHITE       RGB(255, 255, 255)
#define COLOR_RED         RGB(255, 0, 0)
#define COLOR_GREEN       RGB(0, 255, 0)
#define COLOR_BLUE        RGB(0, 0, 255)
#define COLOR_YELLOW      RGB(255, 255, 0)
#define COLOR_CYAN        RGB(0, 255, 255)
#define COLOR_MAGENTA     RGB(255, 0, 255)
#define COLOR_GRAY        RGB(128, 128, 128)
#define COLOR_DARK_GRAY   RGB(64, 64, 64)
#define COLOR_LIGHT_GRAY  RGB(192, 192, 192)
#define COLOR_ORANGE      RGB(255, 165, 0)
#define COLOR_PURPLE      RGB(128, 0, 128)
#define COLOR_PINK        RGB(255, 192, 203)
#define COLOR_BROWN       RGB(139, 69, 19)

// Desktop-style colors
#define COLOR_DESKTOP_BG  RGB(0, 128, 128)  // Teal
#define COLOR_WINDOW_BG   RGB(192, 192, 192)
#define COLOR_TITLE_BAR   RGB(0, 0, 128)    // Dark blue
#define COLOR_TITLE_TEXT  RGB(255, 255, 255)
