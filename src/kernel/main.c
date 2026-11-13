#include "graphics.h"
#include "i8259.h"
#include "idt.h"
#include "isr.h"
#include "keyboard.h"
#include "multiboot.h"
#include "paging.h"
#include "shell.h"
#include "stdint.h"
#include "vga.h"

// VGA text mode constants
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000
#define TERMINAL_FONT_WIDTH 8
#define TERMINAL_FONT_HEIGHT 16

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

typedef enum {
  CONSOLE_FRAMEBUFFER,
  CONSOLE_VGA_COLOR,
  CONSOLE_MONOCHROME
} ConsoleBackend;

static ConsoleBackend console_backend = CONSOLE_VGA_COLOR;
static int terminal_x = 0;
static int terminal_y = 0;
static int terminal_width = VGA_WIDTH;
static int terminal_height = VGA_HEIGHT;

#define SERIAL_COM1 0x3F8

static int console_width(void) {
  if (console_backend == CONSOLE_FRAMEBUFFER) {
    FramebufferInfo *fb = graphics_get_info();
    (void)fb;
    return terminal_width;
  }
  return VGA_WIDTH;
}

static int console_height(void) {
  if (console_backend == CONSOLE_FRAMEBUFFER) {
    FramebufferInfo *fb = graphics_get_info();
    (void)fb;
    return terminal_height;
  }
  return VGA_HEIGHT;
}

// Cursor accessors for shell
int get_cursor_row(void) { return cursor_row; }
int get_cursor_col(void) { return cursor_col; }

// Helper for port I/O
static inline void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
  uint8_t value;
  __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
  return value;
}

static void serial_init(void) {
  outb(SERIAL_COM1 + 1, 0x00);
  outb(SERIAL_COM1 + 3, 0x80);
  outb(SERIAL_COM1 + 0, 0x03);
  outb(SERIAL_COM1 + 1, 0x00);
  outb(SERIAL_COM1 + 3, 0x03);
  outb(SERIAL_COM1 + 2, 0xC7);
  outb(SERIAL_COM1 + 4, 0x0B);
}

static void serial_putc(char c) {
  for (uint32_t timeout = 100000; timeout > 0; timeout--) {
    if (inb(SERIAL_COM1 + 5) & 0x20)
      break;
    __asm__ volatile("pause");
  }

  outb(SERIAL_COM1, (uint8_t)c);
}


static void graphics_terminal_init(void) {
  FramebufferInfo *fb = graphics_get_info();
  int window_x = 24;
  int window_y = 24;
  int window_w = (int)fb->width - 48;
  int window_h = (int)fb->height - 48;

  if (window_w < 160)
    window_w = (int)fb->width;
  if (window_h < 80)
    window_h = (int)fb->height;
  if (window_w == (int)fb->width)
    window_x = 0;
  if (window_h == (int)fb->height)
    window_y = 0;

  graphics_clear(COLOR_DESKTOP_BG);
  graphics_fill_rect(window_x + 4, window_y + 4, window_w, window_h,
                     COLOR_DARK_GRAY);
  graphics_fill_rect(window_x, window_y, window_w, window_h,
                     COLOR_BLACK);
  graphics_fill_rect(window_x, window_y, window_w, 24, COLOR_TITLE_BAR);
  graphics_draw_string(window_x + 10, window_y + 4, "NBOS Terminal",
                       COLOR_WHITE, COLOR_TITLE_BAR);
  graphics_draw_rect(window_x, window_y, window_w, window_h, COLOR_LIGHT_GRAY);

  terminal_x = window_x + 12;
  terminal_y = window_y + 34;
  terminal_width = (window_w - 24) / TERMINAL_FONT_WIDTH;
  terminal_height = (window_h - 46) / TERMINAL_FONT_HEIGHT;
  if (terminal_width < 1)
    terminal_width = 1;
  if (terminal_height < 1)
    terminal_height = 1;
}

// Update hardware cursor position
void update_cursor(void) {
  if (console_backend == CONSOLE_FRAMEBUFFER ||
      console_backend == CONSOLE_MONOCHROME)
    return;

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
  if (console_backend == CONSOLE_FRAMEBUFFER) {
    int content_width = terminal_width * TERMINAL_FONT_WIDTH;
    int content_height = terminal_height * TERMINAL_FONT_HEIGHT;
    for (int y = TERMINAL_FONT_HEIGHT; y < content_height; y++) {
      for (int x = 0; x < content_width; x++) {
        graphics_put_pixel(terminal_x + x, terminal_y + y - TERMINAL_FONT_HEIGHT,
                           graphics_get_pixel(terminal_x + x,
                                              terminal_y + y));
      }
    }
    for (int y = content_height - TERMINAL_FONT_HEIGHT; y < content_height;
         y++) {
      for (int x = 0; x < content_width; x++)
        graphics_put_pixel(terminal_x + x, terminal_y + y, COLOR_BLACK);
    }
    cursor_row = console_height() - 1;
    cursor_col = 0;
    return;
  }

  if (console_backend == CONSOLE_MONOCHROME)
    return;

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
  if (console_backend == CONSOLE_FRAMEBUFFER) {
    (void)color;
    graphics_terminal_init();
    cursor_row = 0;
    cursor_col = 0;
    return;
  }

  if (console_backend == CONSOLE_MONOCHROME) {
    serial_putc('\f');
    cursor_row = 0;
    cursor_col = 0;
    return;
  }

  for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2) {
    video_memory[i] = ' ';
    video_memory[i + 1] = color;
  }
  cursor_row = 0;
  cursor_col = 0;
}

// Print a character at current cursor position
void putchar_at(char c, uint8_t color, int col, int row) {
  if (console_backend == CONSOLE_FRAMEBUFFER) {
    (void)color;
    graphics_draw_char(terminal_x + col * TERMINAL_FONT_WIDTH,
                       terminal_y + row * TERMINAL_FONT_HEIGHT, c, COLOR_WHITE,
                       COLOR_BLACK);
    return;
  }

  if (console_backend == CONSOLE_MONOCHROME) {
    serial_putc(c);
    return;
  }

  int offset = (row * VGA_WIDTH + col) * 2;
  video_memory[offset] = c;
  video_memory[offset + 1] = color;
}

// Print a character and advance cursor
void kputc(char c, uint8_t color) {
  if (console_backend == CONSOLE_MONOCHROME &&
      (c == '\n' || c == '\r' || c == '\b')) {
    serial_putc(c);
  }

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
    if (cursor_col >= console_width()) {
      cursor_col = 0;
      cursor_row++;
    }
  }

  // Scroll screen if we've reached the bottom
  if (cursor_row >= console_height()) {
    scroll_screen();
    cursor_row = console_height() - 1;
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
  if (cursor_row >= console_height()) {
    scroll_screen();
    cursor_row = console_height() - 1;
  }
}

// Restore VGA mode 03h so the existing text shell is visible after the
// graphical welcome screen.
static void restore_vga_text_mode(void) {
  static const uint8_t sequencer[] = {0x03, 0x00, 0x03, 0x00, 0x02};
  static const uint8_t crtc[] = {
      0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F, 0x00, 0x4F,
      0x0D, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x9C, 0x8E, 0x8F, 0x28,
      0x1F, 0x96, 0xB9, 0xA3, 0xFF};
  static const uint8_t graphics_controller[] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF};
  static const uint8_t attribute_controller[] = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
      0x0C, 0x00, 0x0F, 0x08, 0x00};

  outb(0x3C2, 0x67);
  for (uint8_t index = 0; index < sizeof(sequencer); index++) {
    outb(0x3C4, index);
    outb(0x3C5, sequencer[index]);
  }

  outb(0x3D4, 0x11);
  outb(0x3D5, 0x00);
  for (uint8_t index = 0; index < sizeof(crtc); index++) {
    outb(0x3D4, index);
    outb(0x3D5, crtc[index]);
  }

  for (uint8_t index = 0; index < sizeof(graphics_controller); index++) {
    outb(0x3CE, index);
    outb(0x3CF, graphics_controller[index]);
  }

  (void)inb(0x3DA);
  for (uint8_t index = 0; index < sizeof(attribute_controller); index++) {
    outb(0x3C0, index);
    outb(0x3C0, attribute_controller[index]);
  }
  outb(0x3C0, 0x20);
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

  // Initialize Multiboot (parse framebuffer info from GRUB)
  multiboot_init((uint32_t)magic, mbi);

  // Initialize graphics from the custom handoff or parsed Multiboot data.
  graphics_init();
  serial_init();
  if (graphics_is_available()) {
    console_backend = CONSOLE_FRAMEBUFFER;
  } else if (vga_is_available()) {
    console_backend = CONSOLE_VGA_COLOR;
  } else {
    console_backend = CONSOLE_MONOCHROME;
  }

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
  if (console_backend == CONSOLE_FRAMEBUFFER) {
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
    
  }

  if (console_backend == CONSOLE_VGA_COLOR)
    restore_vga_text_mode();

  // Color scheme for kernel messages
  uint8_t title_color = VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
  uint8_t info_color = VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  uint8_t ok_color = VGA_ENTRY_COLOR(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
  uint8_t header_color = VGA_ENTRY_COLOR(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
  uint8_t detail_color = VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK);

  // Initialize the selected terminal backend.
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
  if (console_backend == CONSOLE_FRAMEBUFFER) {
    kprint("[INFO] Graphical framebuffer terminal active", info_color);
  } else if (console_backend == CONSOLE_VGA_COLOR) {
    kprint("[INFO] VGA color text terminal active (80x25)", info_color);
  } else {
    kprint("[INFO] Monochrome serial terminal fallback active", info_color);
  }
  knewline();
  if (console_backend == CONSOLE_VGA_COLOR) {
    kprint("[INFO] Video memory at 0xB8000", info_color);
  } else if (console_backend == CONSOLE_FRAMEBUFFER) {
    kprint("[INFO] Framebuffer text renderer active", info_color);
  } else {
    kprint("[INFO] COM1 serial output at 115200 baud", info_color);
  }
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
