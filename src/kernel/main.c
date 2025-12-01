#include "graphics.h"
#include "i8259.h"
#include "idt.h"
#include "isr.h"
#include "keyboard.h"
#include "multiboot.h"
#include "paging.h"
#include "shell.h"
#include "stdint.h"

// VGA text mode constants
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

// VGA color attributes
#define VGA_COLOR_BLACK 0x0
#define VGA_COLOR_BLUE 0x1
#define VGA_COLOR_GREEN 0x2
#define VGA_COLOR_CYAN 0x3
#define VGA_COLOR_RED 0x4
#define VGA_COLOR_MAGENTA 0x5
#define VGA_COLOR_BROWN 0x6
#define VGA_COLOR_LIGHT_GRAY 0x7
#define VGA_COLOR_DARK_GRAY 0x8
#define VGA_COLOR_LIGHT_BLUE 0x9
#define VGA_COLOR_LIGHT_GREEN 0xA
#define VGA_COLOR_LIGHT_CYAN 0xB
#define VGA_COLOR_LIGHT_RED 0xC
#define VGA_COLOR_LIGHT_MAGENTA 0xD
#define VGA_COLOR_YELLOW 0xE
#define VGA_COLOR_WHITE 0xF

// Create VGA color attribute byte
#define VGA_ENTRY_COLOR(fg, bg) ((bg << 4) | fg)

// Global cursor position
static int cursor_row = 0;
static int cursor_col = 0;
static volatile uint8_t *video_memory = (volatile uint8_t *)VGA_MEMORY;

// Cursor accessors for shell
int get_cursor_row(void) { return cursor_row; }
int get_cursor_col(void) { return cursor_col; }

// Helper for port I/O
static inline void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Update hardware cursor position
void update_cursor(void) {
  uint16_t pos = cursor_row * VGA_WIDTH + cursor_col;
  outb(0x3D4, 0x0F);
  outb(0x3D5, (uint8_t)(pos & 0xFF));
  outb(0x3D4, 0x0E);
  outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void set_cursor_row(int row) {
  cursor_row = row;
  update_cursor();
}

void set_cursor_col(int col) {
  cursor_col = col;
  update_cursor();
}

// Scroll the screen content up by one line
void scroll_screen(void) {
  // Copy all lines up by one
  for (int row = 1; row < VGA_HEIGHT; row++) {
    for (int col = 0; col < VGA_WIDTH * 2; col++) {
      int src_offset = (row * VGA_WIDTH * 2) + col;
      int dst_offset = ((row - 1) * VGA_WIDTH * 2) + col;
      video_memory[dst_offset] = video_memory[src_offset];
    }
  }

  // Clear the last line
  int last_row_offset = (VGA_HEIGHT - 1) * VGA_WIDTH * 2;
  for (int col = 0; col < VGA_WIDTH; col++) {
    video_memory[last_row_offset + col * 2] = ' ';
    video_memory[last_row_offset + col * 2 + 1] =
        VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK);
  }
}

// Clear the screen with specified color
void clear_screen(uint8_t color) {
  for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2) {
    video_memory[i] = ' ';
    video_memory[i + 1] = color;
  }
  cursor_row = 0;
  cursor_col = 0;
}

// Print a character at current cursor position
void putchar_at(char c, uint8_t color, int col, int row) {
  int offset = (row * VGA_WIDTH + col) * 2;
  video_memory[offset] = c;
  video_memory[offset + 1] = color;
}

// Print a character and advance cursor
void kputc(char c, uint8_t color) {
  if (c == '\n') {
    cursor_col = 0;
    cursor_row++;
  } else if (c == '\r') {
    cursor_col = 0;
  } else if (c == '\b') {
    // Backspace: move cursor back and clear character
    if (cursor_col > 0) {
      cursor_col--;
      putchar_at(' ', color, cursor_col, cursor_row);
    }
  } else {
    putchar_at(c, color, cursor_col, cursor_row);
    cursor_col++;
    if (cursor_col >= VGA_WIDTH) {
      cursor_col = 0;
      cursor_row++;
    }
  }

  // Scroll screen if we've reached the bottom
  if (cursor_row >= VGA_HEIGHT) {
    scroll_screen();
    cursor_row = VGA_HEIGHT - 1;
  }
  // Update hardware cursor
  update_cursor();
}

// Print a string with specified color
void kprint(const char *str, uint8_t color) {
  while (*str) {
    kputc(*str, color);
    str++;
  }
}

// Print a newline
void knewline(void) {
  cursor_col = 0;
  cursor_row++;
  // Scroll screen if we've reached the bottom
  if (cursor_row >= VGA_HEIGHT) {
    scroll_screen();
    cursor_row = VGA_HEIGHT - 1;
  }
}

// Print a horizontal line
void kprint_line(char c, int count, uint8_t color) {
  for (int i = 0; i < count; i++) {
    kputc(c, color);
  }
}

void start64(uint64_t magic, uint64_t mbi_addr) {
  // Cast mbi_addr to pointer (we'll use it if needed)
  multiboot_info_t *mbi = (multiboot_info_t *)(uintptr_t)mbi_addr;
  (void)mbi; // Suppress unused warning for now

  // Verify Multiboot magic (lower 32 bits)
  if ((uint32_t)magic != MULTIBOOT_BOOTLOADER_MAGIC) {
    // Not booted by multiboot - continue anyway for custom bootloader
    // The custom bootloader passes 0x2BADB002 in EDI
  }

  // Initialize graphics system first
  graphics_init();

  // Initialize Multiboot (parse framebuffer info from GRUB)
  multiboot_init((uint32_t)magic, mbi);

  // Initialize IDT and ISRs
  idt_init();
  isr_init();

  // Initialize PIC
  i8259_init();

  // Enable interrupts
  __asm__ volatile("sti");

  // Initialize keyboard
  keyboard_init();

  // Check if we're in graphics mode
  if (graphics_is_available()) {
    // Graphical boot screen!
    FramebufferInfo *fb = graphics_get_info();
    
    // Clear screen with a nice gradient-like background
    graphics_clear(COLOR_DESKTOP_BG);
    
    // Draw a centered window
    int win_w = 500;
    int win_h = 300;
    int win_x = (fb->width - win_w) / 2;
    int win_y = (fb->height - win_h) / 2;
    
    // Window shadow
    graphics_fill_rect(win_x + 4, win_y + 4, win_w, win_h, COLOR_DARK_GRAY);
    
    // Window background
    graphics_fill_rect(win_x, win_y, win_w, win_h, COLOR_WINDOW_BG);
    
    // Title bar
    graphics_fill_rect(win_x, win_y, win_w, 24, COLOR_TITLE_BAR);
    graphics_draw_string(win_x + 10, win_y + 4, "NBOS Kernel", COLOR_WHITE, COLOR_TITLE_BAR);
    
    // Window border
    graphics_draw_rect(win_x, win_y, win_w, win_h, COLOR_BLACK);
    
    // Content
    int content_y = win_y + 40;
    int content_x = win_x + 20;
    
    graphics_draw_string(content_x, content_y, "Welcome to NBOS!", COLOR_BLACK, COLOR_WINDOW_BG);
    content_y += 24;
    
    graphics_draw_string(content_x, content_y, "64-bit Long Mode Active", COLOR_DARK_GRAY, COLOR_WINDOW_BG);
    content_y += 20;
    
    // Display resolution info
    int w = fb->width;
    int h = fb->height;
    int b = fb->bpp;
    
    graphics_draw_string(content_x, content_y, "Resolution:", COLOR_BLACK, COLOR_WINDOW_BG);
    content_y += 20;
    
    // Draw resolution numbers manually
    char buf[32];
    int pos = 0;
    
    // Width
    if (w >= 1000) buf[pos++] = '0' + (w / 1000) % 10;
    if (w >= 100) buf[pos++] = '0' + (w / 100) % 10;
    if (w >= 10) buf[pos++] = '0' + (w / 10) % 10;
    buf[pos++] = '0' + w % 10;
    buf[pos++] = 'x';
    
    // Height
    if (h >= 1000) buf[pos++] = '0' + (h / 1000) % 10;
    if (h >= 100) buf[pos++] = '0' + (h / 100) % 10;
    if (h >= 10) buf[pos++] = '0' + (h / 10) % 10;
    buf[pos++] = '0' + h % 10;
    buf[pos++] = 'x';
    
    // BPP
    if (b >= 10) buf[pos++] = '0' + (b / 10) % 10;
    buf[pos++] = '0' + b % 10;
    buf[pos++] = '\0';
    
    graphics_draw_string(content_x + 20, content_y, buf, COLOR_BLUE, COLOR_WINDOW_BG);
    content_y += 30;
    
    // Status
    graphics_draw_string(content_x, content_y, "[OK] System initialized", COLOR_GREEN, COLOR_WINDOW_BG);
    content_y += 20;
    graphics_draw_string(content_x, content_y, "[OK] Interrupts enabled", COLOR_GREEN, COLOR_WINDOW_BG);
    content_y += 20;
    graphics_draw_string(content_x, content_y, "[OK] Keyboard ready", COLOR_GREEN, COLOR_WINDOW_BG);
    content_y += 30;
    
    graphics_draw_string(content_x, content_y, "Press any key to continue...", COLOR_DARK_GRAY, COLOR_WINDOW_BG);
    
    // Wait for a keypress
    // Simple busy wait for keyboard
    while (!keyboard_has_key()) {
      __asm__ volatile("hlt");
    }
    keyboard_get_key(); // Consume the key
    
    // Clear for shell (fallback to text mode for now)
    // For a real graphical shell, we'd need a lot more code!
  }

  // Color scheme for kernel messages
  uint8_t title_color = VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
  uint8_t info_color = VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  uint8_t ok_color = VGA_ENTRY_COLOR(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
  uint8_t header_color = VGA_ENTRY_COLOR(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
  uint8_t detail_color = VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK);

  // Clear screen with black background
  clear_screen(VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));

  // Print kernel header
  kprint_line('=', 50, header_color);
  knewline();
  kprint("          KERNEL STARTED SUCCESSFULLY", title_color);
  knewline();
  kprint_line('=', 50, header_color);
  knewline();
  knewline();

  // Print system information
  kprint("[INFO] Kernel loaded and running in 64-bit Long Mode", info_color);
  knewline();
  kprint("[INFO] VGA text mode initialized (80x25)", info_color);
  knewline();
  kprint("[INFO] Video memory at 0xB8000", info_color);
  knewline();
  knewline();

  // Print status
  kprint("[OK] System initialization complete", ok_color);
  knewline();
  knewline();

  // Print details section
  kprint_line('-', 50, detail_color);
  knewline();
  kprint("System Details:", header_color);
  knewline();
  kprint("  - Architecture: x86_64 (AMD64)", detail_color);
  knewline();
  kprint("  - Mode: 64-bit Long Mode", detail_color);
  knewline();
  kprint("  - Kernel Address: 0x100000 (1MB)", detail_color);
  knewline();
  kprint("  - Version: 1.0.0", detail_color);
  knewline();
  kprint("  - Build Date: 2025-12-01", detail_color);
  knewline();
  kprint_line('-', 50, detail_color);
  knewline();
  knewline();

  // Initialize shell
  shell_init();

  // Run shell (never returns)
  shell_run();

  // Halt
  for (;;)
    __asm__("hlt");
}
