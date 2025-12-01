#include "shell.h"
#include "paging.h"
#include "stdint.h"
#include "console.h"
#include "heap.h"
#include "syscall.h"
#include "graphics.h"

// External functions from main.c (for VGA fallback)
extern void kputc(char c, uint8_t color);
extern void kprint(const char *str, uint8_t color);
extern void knewline(void);
extern void clear_screen(uint8_t color);
extern int get_cursor_col(void);
extern int get_cursor_row(void);
extern void set_cursor_col(int col);
extern void set_cursor_row(int row);

// Check if we're in graphics mode
extern int is_graphics_mode();

// GUI Terminal state
static int gui_terminal_x = 0;
static int gui_terminal_y = 0;
static int gui_terminal_w = 0;
static int gui_terminal_h = 0;
static int gui_content_x = 0;
static int gui_content_y = 0;
static int gui_content_w = 0;
static int gui_content_h = 0;
static int gui_title_bar_height = 24;
static int gui_border_width = 4;
static int gui_mode = 0;  // 1 if GUI terminal is active

// Helper for port I/O
static inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

// VGA colors
#define VGA_COLOR_BLACK 0x0
#define VGA_COLOR_LIGHT_GRAY 0x7
#define VGA_COLOR_LIGHT_GREEN 0xA
#define VGA_COLOR_LIGHT_RED 0xC
#define VGA_COLOR_YELLOW 0xE
#define VGA_COLOR_WHITE 0xF
#define VGA_ENTRY_COLOR(fg, bg) ((bg << 4) | fg)

// Input buffer
#define INPUT_BUFFER_SIZE 256
static char input_buffer[INPUT_BUFFER_SIZE];
static int input_pos = 0;

// String functions
static int strlen(const char *str) {
  int len = 0;
  while (str[len])
    len++;
  return len;
}

// Draw the GUI terminal window
static void draw_gui_terminal(void) {
  if (!is_graphics_mode() || !graphics_is_available()) {
    return;
  }
  
  FramebufferInfo *fb = graphics_get_info();
  if (!fb || fb->framebuffer_addr == 0 || fb->width == 0 || fb->height == 0) {
    return;  // Safety check
  }
  
  int screen_w = fb->width;
  int screen_h = fb->height;
  
  // Calculate terminal window size (nearly full screen with margin)
  int margin = 20;
  gui_terminal_x = margin;
  gui_terminal_y = margin;
  gui_terminal_w = screen_w - margin * 2;
  gui_terminal_h = screen_h - margin * 2;
  
  // Calculate content area (inside window, below title bar)
  gui_content_x = gui_terminal_x + gui_border_width + 2;
  gui_content_y = gui_terminal_y + gui_title_bar_height + gui_border_width + 2;
  gui_content_w = gui_terminal_w - (gui_border_width + 2) * 2;
  gui_content_h = gui_terminal_h - gui_title_bar_height - (gui_border_width + 2) * 2;
  
  // Draw desktop background
  graphics_clear(COLOR_DESKTOP_BG);
  
  // Draw the window with title bar
  graphics_draw_window(gui_terminal_x, gui_terminal_y, gui_terminal_w, gui_terminal_h,
                       "project_OS Terminal", COLOR_TITLE_BAR, COLOR_WINDOW_BG);
  
  // Draw a sunken content area (terminal text area with black background)
  graphics_draw_panel(gui_content_x - 2, gui_content_y - 2, 
                      gui_content_w + 4, gui_content_h + 4,
                      0x000000, PANEL_SUNKEN);
  
  gui_mode = 1;
}

// Custom console functions that respect GUI terminal bounds
static int gui_cursor_row = 0;
static int gui_cursor_col = 0;
static int gui_console_cols = 0;
static int gui_console_rows = 0;

// Cursor state
static int cursor_visible = 0;
static uint32_t cursor_blink_counter = 0;
#define CURSOR_BLINK_RATE 50000  // Adjust for blink speed

// Draw cursor at current position
static void draw_cursor(int visible) {
  if (!gui_mode) return;
  
  int x = gui_content_x + gui_cursor_col * 8;
  int y = gui_content_y + gui_cursor_row * 16;
  
  if (visible) {
    // Draw a block cursor (inverted colors)
    graphics_fill_rect(x, y + 14, 8, 2, 0x00FF00);  // Green underscore cursor
  } else {
    // Erase cursor
    graphics_fill_rect(x, y + 14, 8, 2, 0x000000);
  }
}

// Toggle cursor visibility (called periodically)
static void toggle_cursor(void) {
  cursor_visible = !cursor_visible;
  draw_cursor(cursor_visible);
}

// Hide cursor before moving/typing
static void hide_cursor(void) {
  if (cursor_visible) {
    draw_cursor(0);
    cursor_visible = 0;
  }
}

// Show cursor after moving/typing
static void show_cursor(void) {
  draw_cursor(1);
  cursor_visible = 1;
}

static void gui_console_init(void) {
  gui_cursor_row = 0;
  gui_cursor_col = 0;
  gui_console_cols = gui_content_w / 8;  // 8 pixels per char
  gui_console_rows = gui_content_h / 16; // 16 pixels per char
}

static void gui_console_scroll(void) {
  if (!graphics_is_available()) return;
  
  FramebufferInfo *fb = graphics_get_info();
  if (!fb || fb->framebuffer_addr == 0) return;
  
  int bytes_per_pixel = fb->bpp / 8;
  if (bytes_per_pixel < 3 || bytes_per_pixel > 4) return;  // Sanity check
  
  // Use a safer approach: read/write pixels through the graphics API
  // This is slower but safer than direct framebuffer manipulation
  for (int row = 0; row < gui_console_rows - 1; row++) {
    int dst_y = gui_content_y + row * 16;
    int src_y = gui_content_y + (row + 1) * 16;
    
    for (int line = 0; line < 16; line++) {
      for (int x = 0; x < gui_content_w; x++) {
        uint32_t pixel = graphics_get_pixel(gui_content_x + x, src_y + line);
        graphics_put_pixel(gui_content_x + x, dst_y + line, pixel);
      }
    }
  }
  
  // Clear last row
  int last_row_y = gui_content_y + (gui_console_rows - 1) * 16;
  graphics_fill_rect(gui_content_x, last_row_y, gui_content_w, 16, 0x000000);
}

static void gui_console_putchar(char c, uint32_t color) {
  if (!gui_mode) return;
  
  // Hide cursor before modifying
  hide_cursor();
  
  if (c == '\n') {
    gui_cursor_col = 0;
    gui_cursor_row++;
  } else if (c == '\r') {
    gui_cursor_col = 0;
  } else if (c == '\b') {
    if (gui_cursor_col > 0) {
      gui_cursor_col--;
      int x = gui_content_x + gui_cursor_col * 8;
      int y = gui_content_y + gui_cursor_row * 16;
      graphics_fill_rect(x, y, 8, 16, 0x000000);
    }
  } else if (c == '\t') {
    gui_cursor_col = (gui_cursor_col + 8) & ~7;
    if (gui_cursor_col >= gui_console_cols) {
      gui_cursor_col = 0;
      gui_cursor_row++;
    }
  } else {
    int x = gui_content_x + gui_cursor_col * 8;
    int y = gui_content_y + gui_cursor_row * 16;
    graphics_draw_char(x, y, c, color, 0x000000);
    gui_cursor_col++;
    
    if (gui_cursor_col >= gui_console_cols) {
      gui_cursor_col = 0;
      gui_cursor_row++;
    }
  }
  
  // Handle scrolling
  if (gui_cursor_row >= gui_console_rows) {
    gui_console_scroll();
    gui_cursor_row = gui_console_rows - 1;
  }
  
  // Show cursor at new position
  show_cursor();
}

static void gui_console_print(const char *str, uint32_t color) {
  while (*str) {
    gui_console_putchar(*str, color);
    str++;
  }
}

static void gui_console_newline(void) {
  gui_cursor_col = 0;
  gui_cursor_row++;
  
  if (gui_cursor_row >= gui_console_rows) {
    gui_console_scroll();
    gui_cursor_row = gui_console_rows - 1;
  }
}

// Wrapper functions for output (use GUI console if available)
static void shell_print(const char *str, uint32_t color) {
  if (gui_mode) {
    gui_console_print(str, color);
  } else if (is_graphics_mode()) {
    console_print(str, color);
  } else {
    // Map 32-bit color to VGA color (simplified)
    uint8_t vga_color = VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    if (color == CONSOLE_COLOR_GREEN || color == CONSOLE_COLOR_LIGHT_GREEN) {
      vga_color = VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    } else if (color == CONSOLE_COLOR_YELLOW || color == CONSOLE_COLOR_ORANGE) {
      vga_color = VGA_ENTRY_COLOR(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    } else if (color == CONSOLE_COLOR_RED || color == CONSOLE_COLOR_LIGHT_RED) {
      vga_color = VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    } else if (color == CONSOLE_COLOR_GRAY || color == CONSOLE_COLOR_LIGHT_GRAY || color == CONSOLE_COLOR_DARK_GRAY) {
      vga_color = VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK);
    }
    kprint(str, vga_color);
  }
}

static void shell_putc(char c, uint32_t color) {
  if (gui_mode) {
    gui_console_putchar(c, color);
  } else if (is_graphics_mode()) {
    console_putchar(c, color);
  } else {
    uint8_t vga_color = VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kputc(c, vga_color);
  }
}

static void shell_newline(void) {
  if (gui_mode) {
    gui_console_newline();
  } else if (is_graphics_mode()) {
    console_newline();
  } else {
    knewline();
  }
}

static void shell_clear(void) {
  if (gui_mode) {
    // Redraw the GUI terminal
    draw_gui_terminal();
    gui_console_init();
  } else if (is_graphics_mode()) {
    console_clear(CONSOLE_COLOR_BLACK);
  } else {
    clear_screen(VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  }
}

static int shell_get_col(void) {
  if (gui_mode) {
    return gui_cursor_col;
  } else if (is_graphics_mode()) {
    return console_get_col();
  } else {
    return get_cursor_col();
  }
}

static int shell_get_row(void) {
  if (gui_mode) {
    return gui_cursor_row;
  } else if (is_graphics_mode()) {
    return console_get_row();
  } else {
    return get_cursor_row();
  }
}

static void shell_set_col(int col) {
  if (gui_mode) {
    hide_cursor();
    if (col >= 0 && col < gui_console_cols) {
      gui_cursor_col = col;
    }
    show_cursor();
  } else if (is_graphics_mode()) {
    console_set_col(col);
  } else {
    set_cursor_col(col);
  }
}

static void shell_set_row(int row) {
  if (gui_mode) {
    hide_cursor();
    if (row >= 0 && row < gui_console_rows) {
      gui_cursor_row = row;
    }
    show_cursor();
  } else if (is_graphics_mode()) {
    console_set_row(row);
  } else {
    set_cursor_row(row);
  }
}

static int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static int strncmp(const char *s1, const char *s2, int n) {
  while (n && *s1 && (*s1 == *s2)) {
    s1++;
    s2++;
    n--;
  }
  if (n == 0)
    return 0;
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

// Display prompt
static void show_prompt(void) {
  shell_print("$ ", CONSOLE_COLOR_LIGHT_GREEN);
}

// Built-in command: help
static void cmd_help(void) {
  shell_newline();
  shell_print("Available commands:\n", CONSOLE_COLOR_YELLOW);
  shell_print("  help   - Show this help message\n", CONSOLE_COLOR_WHITE);
  shell_print("  clear  - Clear the screen\n", CONSOLE_COLOR_WHITE);
  shell_print("  echo   - Print text to screen\n", CONSOLE_COLOR_WHITE);
  shell_print("  peek   - Inspect memory (peek <addr>)\n", CONSOLE_COLOR_WHITE);
  shell_print("  mem    - Show memory statistics\n", CONSOLE_COLOR_WHITE);
  shell_print("  alloc  - Test allocation (alloc <size>)\n", CONSOLE_COLOR_WHITE);
  shell_print("  linux  - Linux syscall compat layer\n", CONSOLE_COLOR_WHITE);
  shell_print("  reboot - Reboot the system\n", CONSOLE_COLOR_WHITE);
  shell_print("  shutdown - Power off the system\n", CONSOLE_COLOR_WHITE);
  shell_print("  halt   - Halt the system\n", CONSOLE_COLOR_WHITE);
  shell_print("  about  - Show OS information\n", CONSOLE_COLOR_WHITE);
  shell_print("  info   - Show system register info\n", CONSOLE_COLOR_WHITE);
  shell_print("  syscall - Test syscall interface\n", CONSOLE_COLOR_WHITE);
}

// Built-in command: clear
static void cmd_clear(void) {
  shell_clear();
}

// Built-in command: echo
static void cmd_echo(const char *args) {
  shell_newline();
  shell_print(args, CONSOLE_COLOR_WHITE);
  shell_newline();
}

// Helper for port I/O
static inline void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
  __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

// Helper to parse hex string
static uint64_t atoi_hex(const char *s) {
  uint64_t res = 0;
  while (*s) {
    char c = *s;
    if (c >= '0' && c <= '9')
      res = res * 16 + (c - '0');
    else if (c >= 'a' && c <= 'f')
      res = res * 16 + (c - 'a' + 10);
    else if (c >= 'A' && c <= 'F')
      res = res * 16 + (c - 'A' + 10);
    s++;
  }
  return res;
}

// Helper to print hex byte
static void print_hex_byte(uint8_t byte) {
  char hex[] = "0123456789ABCDEF";
  shell_putc(hex[(byte >> 4) & 0xF], CONSOLE_COLOR_WHITE);
  shell_putc(hex[byte & 0xF], CONSOLE_COLOR_WHITE);
}

// Built-in command: reboot
static void cmd_reboot(void) {
  shell_newline();
  shell_print("Rebooting...", CONSOLE_COLOR_LIGHT_RED);

  // Pulse the CPU reset line using the keyboard controller
  uint8_t good = 0x02;
  while (good & 0x02)
    good = inb(0x64);
  outb(0x64, 0xFE);

  // If that fails, try Triple Fault
  __asm__ volatile("lidt (%0)" : : "r"((uint64_t)0));
  __asm__ volatile("int3");

  // Halt if all else fails
  for (;;)
    ;
}

// Built-in command: shutdown
static void cmd_shutdown(void) {
  shell_newline();
  shell_print("Shutting down...", CONSOLE_COLOR_LIGHT_RED);

  // QEMU Shutdown (older versions)
  outw(0xB004, 0x2000);

  // QEMU Shutdown (newer versions)
  outw(0x604, 0x2000);

  // VirtualBox Shutdown
  outw(0x4004, 0x3400);

  // Bochs/Old QEMU Shutdown
  // (Requires writing string "Shutdown" to 0x8900 - skipped for brevity as it's
  // complex in C without string loop)

  // If we are still here, it failed
  shell_newline();
  shell_print("Shutdown failed (ACPI not implemented).\n", CONSOLE_COLOR_YELLOW);
  shell_print("System Halted.", CONSOLE_COLOR_LIGHT_RED);

  // Disable interrupts and halt
  __asm__ volatile("cli");
  for (;;) {
    __asm__ volatile("hlt");
  }
}

// Built-in command: halt
static void cmd_halt(void) {
  shell_newline();
  shell_print("System Halted.", CONSOLE_COLOR_LIGHT_RED);
  shell_print(" Press Reset to restart.", CONSOLE_COLOR_GRAY);

  // Disable interrupts and halt
  __asm__ volatile("cli");
  for (;;) {
    __asm__ volatile("hlt");
  }
}

// Built-in command: peek
static void cmd_peek(const char *args) {
  if (!*args) {
    shell_newline();
    shell_print("Usage: peek <address>\n", CONSOLE_COLOR_YELLOW);
    return;
  }

  uint64_t addr = atoi_hex(args);
  uint8_t *ptr = (uint8_t *)(uintptr_t)addr;

  shell_newline();
  shell_print("Memory at 0x", CONSOLE_COLOR_GRAY);
  shell_print(args, CONSOLE_COLOR_WHITE);
  shell_print(": ", CONSOLE_COLOR_GRAY);

  // Print 16 bytes
  for (int i = 0; i < 16; i++) {
    print_hex_byte(ptr[i]);
    shell_putc(' ', CONSOLE_COLOR_GRAY);
  }
  shell_newline();
}

// Flag to indicate we're waiting to dismiss a dialog
static volatile int dialog_active = 0;

// Redraw the GUI terminal (called after closing dialog)
static void redraw_gui_terminal(void) {
  if (!gui_mode) return;
  
  // Redraw the terminal window
  draw_gui_terminal();
  gui_console_init();
}

// Show a GUI message box (centered on screen)
static void show_message_box(const char *title, const char **lines, int line_count) {
  if (!gui_mode || !graphics_is_available()) {
    // Fallback to text mode
    shell_newline();
    shell_print(title, CONSOLE_COLOR_YELLOW);
    shell_newline();
    for (int i = 0; i < line_count; i++) {
      shell_print(lines[i], CONSOLE_COLOR_WHITE);
      shell_newline();
    }
    return;
  }
  
  FramebufferInfo *fb = graphics_get_info();
  int screen_w = fb->width;
  int screen_h = fb->height;
  
  // Calculate box size
  int max_len = strlen(title);
  for (int i = 0; i < line_count; i++) {
    int len = strlen(lines[i]);
    if (len > max_len) max_len = len;
  }
  
  int box_w = (max_len + 4) * 8 + 20;  // char width + padding
  int box_h = (line_count + 4) * 16 + 60;  // lines + title + padding + button
  
  // Ensure minimum size
  if (box_w < 250) box_w = 250;
  if (box_h < 120) box_h = 120;
  
  int box_x = (screen_w - box_w) / 2;
  int box_y = (screen_h - box_h) / 2;
  
  // Draw dialog window
  graphics_draw_window(box_x, box_y, box_w, box_h, title, COLOR_TITLE_BAR, COLOR_WINDOW_BG);
  
  // Draw message lines
  int text_y = box_y + 40;
  for (int i = 0; i < line_count; i++) {
    int text_x = box_x + 20;
    graphics_draw_string(text_x, text_y, lines[i], COLOR_BLACK, COLOR_WINDOW_BG);
    text_y += 18;
  }
  
  // Draw OK button at bottom
  int btn_w = 80;
  int btn_h = 25;
  int btn_x = box_x + (box_w - btn_w) / 2;
  int btn_y = box_y + box_h - btn_h - 15;
  graphics_draw_button(btn_x, btn_y, btn_w, btn_h, "OK", 0);
  
  // Draw hint text
  graphics_draw_string(box_x + 10, box_y + box_h - 35, 
                       "Press any key to close", COLOR_GRAY, COLOR_WINDOW_BG);
  
  // Set dialog active flag
  dialog_active = 1;
}

// Built-in command: about
static void cmd_about(void) {
  const char *lines[] = {
    "A Custom 64-bit Operating System",
    "",
    "Version: 1.0.0",
    "",
    "Features:",
    "  - 64-bit Long Mode",
    "  - VESA Graphics Mode",
    "  - GUI Terminal Interface",
    "  - Interrupt Handling (IDT)",
    "  - PS/2 Keyboard Driver",
    "  - Interactive Shell",
    "  - Syscall Interface"
  };
  
  show_message_box("About project_OS", lines, 12);
}

// Helper to print 32-bit hex value
static void print_hex32(uint32_t val) {
  char hex[] = "0123456789ABCDEF";
  shell_print("0x", CONSOLE_COLOR_WHITE);
  for (int i = 28; i >= 0; i -= 4) {
    shell_putc(hex[(val >> i) & 0xF], CONSOLE_COLOR_WHITE);
  }
}

// Built-in command: info
static void cmd_info(void) {
  shell_newline();
  shell_print("System Information:\n", CONSOLE_COLOR_YELLOW);

  shell_print("  CR0: ", CONSOLE_COLOR_GRAY);
  print_hex32(get_cr0());
  shell_print("  (PG=", CONSOLE_COLOR_GRAY);
  shell_print((get_cr0() & 0x80000000) ? "1" : "0", CONSOLE_COLOR_WHITE);
  shell_print(")\n", CONSOLE_COLOR_GRAY);

  shell_print("  CR3: ", CONSOLE_COLOR_GRAY);
  print_hex32(get_cr3());
  shell_newline();

  shell_print("  CR4: ", CONSOLE_COLOR_GRAY);
  print_hex32(get_cr4());
  shell_print("  (PAE=", CONSOLE_COLOR_GRAY);
  shell_print((get_cr4() & 0x20) ? "1" : "0", CONSOLE_COLOR_WHITE);
  shell_print(")\n", CONSOLE_COLOR_GRAY);
}

// Test syscall interface
static void cmd_syscall(void) {
  shell_newline();
  shell_print("Testing syscall interface (INT 0x80)...\n\n", CONSOLE_COLOR_YELLOW);
  
  // Test SYS_GETPID (7) - should return 1
  uint64_t pid;
  __asm__ volatile(
    "movq $7, %%rax\n"   // SYS_GETPID
    "int $0x80\n"
    : "=a"(pid)
    :
    : "memory"
  );
  
  shell_print("SYS_GETPID result: ", CONSOLE_COLOR_GRAY);
  char buf[16];
  buf[0] = '0' + (pid % 10);
  buf[1] = '\0';
  shell_print(buf, CONSOLE_COLOR_LIGHT_GREEN);
  shell_print(" (expected: 1)\n", CONSOLE_COLOR_GRAY);
  
  // Test SYS_GETWIDTH (13)
  uint64_t width;
  __asm__ volatile(
    "movq $13, %%rax\n"  // SYS_GETWIDTH
    "int $0x80\n"
    : "=a"(width)
    :
    : "memory"
  );
  
  shell_print("SYS_GETWIDTH result: ", CONSOLE_COLOR_GRAY);
  // Simple number to string
  int i = 0;
  uint64_t n = width;
  if (n == 0) {
    buf[i++] = '0';
  } else {
    while (n > 0) {
      buf[i++] = '0' + (n % 10);
      n /= 10;
    }
  }
  buf[i] = '\0';
  // Reverse
  for (int j = 0; j < i / 2; j++) {
    char t = buf[j];
    buf[j] = buf[i - 1 - j];
    buf[i - 1 - j] = t;
  }
  shell_print(buf, CONSOLE_COLOR_LIGHT_GREEN);
  shell_newline();
  
  // Test SYS_GETHEIGHT (14)
  uint64_t height;
  __asm__ volatile(
    "movq $14, %%rax\n"  // SYS_GETHEIGHT
    "int $0x80\n"
    : "=a"(height)
    :
    : "memory"
  );
  
  shell_print("SYS_GETHEIGHT result: ", CONSOLE_COLOR_GRAY);
  i = 0;
  n = height;
  if (n == 0) {
    buf[i++] = '0';
  } else {
    while (n > 0) {
      buf[i++] = '0' + (n % 10);
      n /= 10;
    }
  }
  buf[i] = '\0';
  for (int j = 0; j < i / 2; j++) {
    char t = buf[j];
    buf[j] = buf[i - 1 - j];
    buf[i - 1 - j] = t;
  }
  shell_print(buf, CONSOLE_COLOR_LIGHT_GREEN);
  shell_newline();
  
  shell_print("\nSyscall test complete!\n", CONSOLE_COLOR_LIGHT_GREEN);
}

// Helper to print number
static void print_number(uint64_t n, uint32_t color) {
  char buf[32];
  int pos = 0;
  
  if (n == 0) {
    buf[pos++] = '0';
  } else {
    int digits[20];
    int count = 0;
    while (n > 0) {
      digits[count++] = n % 10;
      n /= 10;
    }
    for (int i = count - 1; i >= 0; i--) {
      buf[pos++] = '0' + digits[i];
    }
  }
  buf[pos] = '\0';
  shell_print(buf, color);
}

// Built-in command: mem - show memory statistics
static void cmd_mem(void) {
  shell_newline();
  shell_print("Memory Statistics:\n", CONSOLE_COLOR_YELLOW);
  
  HeapStats stats;
  heap_get_stats(&stats);
  
  shell_print("  Total heap:    ", CONSOLE_COLOR_GRAY);
  print_number(stats.total_size / 1024, CONSOLE_COLOR_WHITE);
  shell_print(" KB (", CONSOLE_COLOR_GRAY);
  print_number(stats.total_size / (1024 * 1024), CONSOLE_COLOR_WHITE);
  shell_print(" MB)\n", CONSOLE_COLOR_GRAY);
  
  shell_print("  Used:          ", CONSOLE_COLOR_GRAY);
  print_number(stats.used_size, CONSOLE_COLOR_LIGHT_RED);
  shell_print(" bytes\n", CONSOLE_COLOR_GRAY);
  
  shell_print("  Free:          ", CONSOLE_COLOR_GRAY);
  print_number(stats.free_size / 1024, CONSOLE_COLOR_LIGHT_GREEN);
  shell_print(" KB\n", CONSOLE_COLOR_GRAY);
  
  shell_print("  Allocations:   ", CONSOLE_COLOR_GRAY);
  print_number(stats.num_allocations, CONSOLE_COLOR_WHITE);
  shell_newline();
  
  shell_print("  Free blocks:   ", CONSOLE_COLOR_GRAY);
  print_number(stats.num_free_blocks, CONSOLE_COLOR_WHITE);
  shell_newline();
  
  shell_print("  Largest free:  ", CONSOLE_COLOR_GRAY);
  print_number(stats.largest_free / 1024, CONSOLE_COLOR_LIGHT_GREEN);
  shell_print(" KB\n", CONSOLE_COLOR_GRAY);
}

// Helper to parse decimal number
static uint64_t parse_number(const char *s) {
  uint64_t res = 0;
  while (*s >= '0' && *s <= '9') {
    res = res * 10 + (*s - '0');
    s++;
  }
  return res;
}

// Built-in command: alloc - test memory allocation
static void cmd_alloc(const char *args) {
  shell_newline();
  
  if (!*args) {
    shell_print("Usage: alloc <size>\n", CONSOLE_COLOR_YELLOW);
    shell_print("  Allocates <size> bytes and shows result.\n", CONSOLE_COLOR_GRAY);
    shell_print("  Use 'mem' to see current allocations.\n", CONSOLE_COLOR_GRAY);
    return;
  }
  
  uint64_t size = parse_number(args);
  if (size == 0) {
    shell_print("Invalid size\n", CONSOLE_COLOR_LIGHT_RED);
    return;
  }
  
  void *ptr = kmalloc(size);
  
  if (ptr == NULL) {
    shell_print("Allocation failed! Out of memory.\n", CONSOLE_COLOR_LIGHT_RED);
  } else {
    shell_print("Allocated ", CONSOLE_COLOR_LIGHT_GREEN);
    print_number(size, CONSOLE_COLOR_WHITE);
    shell_print(" bytes at 0x", CONSOLE_COLOR_LIGHT_GREEN);
    
    // Print address as hex
    char hex[] = "0123456789ABCDEF";
    uint64_t addr = (uint64_t)ptr;
    char buf[17];
    for (int i = 15; i >= 0; i--) {
      buf[i] = hex[addr & 0xF];
      addr >>= 4;
    }
    buf[16] = '\0';
    // Skip leading zeros
    char *p = buf;
    while (*p == '0' && *(p+1) != '\0') p++;
    shell_print(p, CONSOLE_COLOR_WHITE);
    shell_newline();
    
    // Fill with pattern to verify it works
    uint8_t *bytes = (uint8_t *)ptr;
    for (uint64_t i = 0; i < size; i++) {
      bytes[i] = (uint8_t)(i & 0xFF);
    }
    shell_print("Memory filled with pattern.\n", CONSOLE_COLOR_GRAY);
  }
}

// Built-in command: linux - show Linux syscall compatibility info
static void cmd_linux(const char *args) {
  shell_newline();
  
  if (*args && strcmp(args, "on") == 0) {
    syscall_set_linux_mode(1);
    shell_print("Linux compatibility mode ", CONSOLE_COLOR_WHITE);
    shell_print("ENABLED\n", CONSOLE_COLOR_LIGHT_GREEN);
    shell_print("Syscalls will be interpreted as Linux x86_64 syscalls.\n", CONSOLE_COLOR_GRAY);
    return;
  }
  
  if (*args && strcmp(args, "off") == 0) {
    syscall_set_linux_mode(0);
    shell_print("Linux compatibility mode ", CONSOLE_COLOR_WHITE);
    shell_print("DISABLED\n", CONSOLE_COLOR_YELLOW);
    shell_print("Syscalls will be interpreted as NBOS native syscalls.\n", CONSOLE_COLOR_GRAY);
    return;
  }
  
  // Show current status and info
  shell_print("=== Linux Compatibility Layer ===\n", CONSOLE_COLOR_CYAN);
  shell_newline();
  
  shell_print("Status: ", CONSOLE_COLOR_WHITE);
  if (syscall_get_linux_mode()) {
    shell_print("ENABLED\n", CONSOLE_COLOR_LIGHT_GREEN);
  } else {
    shell_print("DISABLED\n", CONSOLE_COLOR_YELLOW);
  }
  shell_newline();
  
  shell_print("Implemented Linux Syscalls:\n", CONSOLE_COLOR_WHITE);
  shell_print("  read (0)      - Read from stdin\n", CONSOLE_COLOR_GRAY);
  shell_print("  write (1)     - Write to stdout/stderr\n", CONSOLE_COLOR_GRAY);
  shell_print("  brk (12)      - Expand heap (sbrk)\n", CONSOLE_COLOR_GRAY);
  shell_print("  mmap (9)      - Memory mapping (anonymous only)\n", CONSOLE_COLOR_GRAY);
  shell_print("  munmap (11)   - Unmap memory\n", CONSOLE_COLOR_GRAY);
  shell_print("  exit (60)     - Exit program\n", CONSOLE_COLOR_GRAY);
  shell_print("  getpid (39)   - Get process ID\n", CONSOLE_COLOR_GRAY);
  shell_print("  getuid (102)  - Get user ID\n", CONSOLE_COLOR_GRAY);
  shell_print("  getgid (104)  - Get group ID\n", CONSOLE_COLOR_GRAY);
  shell_newline();
  
  shell_print("Usage:\n", CONSOLE_COLOR_WHITE);
  shell_print("  linux on      - Enable Linux mode\n", CONSOLE_COLOR_GRAY);
  shell_print("  linux off     - Disable Linux mode\n", CONSOLE_COLOR_GRAY);
}

// Execute command
static void execute_command(void) {
  // Null-terminate input
  // input_buffer[input_pos] = '\0'; // BUG: This truncates at cursor! Buffer is
  // already null-terminated.

  // Actually, we should ensure the buffer is null terminated at its true end,
  // but shell_putchar maintains that.
  // However, we should check if input_buffer is empty based on strlen, not
  // input_pos? No, input_pos is just cursor.

  int len = strlen(input_buffer);

  // Skip empty commands
  if (len == 0) {
    shell_newline();
    show_prompt();
    return;
  }

  // Parse command
  char *cmd = input_buffer;
  char *args = input_buffer;

  // Find first space
  while (*args && *args != ' ')
    args++;
  if (*args) {
    *args = '\0'; // Null-terminate command
    args++;       // Point to arguments
    while (*args == ' ')
      args++; // Skip leading spaces
  }

  // Execute built-in commands
  if (strcmp(cmd, "help") == 0) {
    cmd_help();
  } else if (strcmp(cmd, "clear") == 0) {
    cmd_clear();
  } else if (strcmp(cmd, "echo") == 0) {
    cmd_echo(args);
  } else if (strcmp(cmd, "reboot") == 0) {
    cmd_reboot();
  } else if (strcmp(cmd, "shutdown") == 0) {
    cmd_shutdown();
  } else if (strcmp(cmd, "halt") == 0) {
    cmd_halt();
  } else if (strcmp(cmd, "peek") == 0) {
    cmd_peek(args);
  } else if (strcmp(cmd, "about") == 0) {
    cmd_about();
  } else if (strcmp(cmd, "info") == 0) {
    cmd_info();
  } else if (strcmp(cmd, "syscall") == 0) {
    cmd_syscall();
  } else if (strcmp(cmd, "mem") == 0) {
    cmd_mem();
  } else if (strcmp(cmd, "alloc") == 0) {
    cmd_alloc(args);
  } else if (strcmp(cmd, "linux") == 0) {
    cmd_linux(args);
  } else {
    shell_newline();
    shell_print("Unknown command: ", CONSOLE_COLOR_YELLOW);
    shell_print(cmd, CONSOLE_COLOR_WHITE);
    shell_newline();
    shell_print("Type 'help' for available commands\n", CONSOLE_COLOR_GRAY);
  }

  // Reset input buffer
  input_pos = 0;
  input_buffer[0] = '\0'; // Clear buffer
  show_prompt();
}

// Helper to redraw line from current position
static void redraw_line(void) {
  int start_col = shell_get_col();
  int start_row = shell_get_row();

  // Print remaining part of buffer
  shell_print(&input_buffer[input_pos], CONSOLE_COLOR_WHITE);

  // Clear extra character at the end (if we deleted)
  shell_putc(' ', CONSOLE_COLOR_WHITE);

  // Restore cursor
  shell_set_row(start_row);
  shell_set_col(start_col);
}

// Handle character input
void shell_putchar(char c) {
  // If a dialog is active, any key dismisses it
  if (dialog_active) {
    dialog_active = 0;
    redraw_gui_terminal();
    shell_print("Welcome to project_OS Shell!\n", CONSOLE_COLOR_LIGHT_GREEN);
    shell_print("Type 'help' for available commands.\n\n", CONSOLE_COLOR_GRAY);
    show_prompt();
    return;
  }
  
  if (c == '\n') {
    // Move cursor to end of line before executing
    int len = strlen(input_buffer);
    if (input_pos < len) {
      // We need to calculate where the end is visually
      // This is tricky if we wrapped lines, but assuming single line for now
      // Simple hack: just print newline
    }
    execute_command();;
  } else if (c == '\b') {
    // Handle backspace
    if (input_pos > 0) {
      // Shift buffer left
      int len = strlen(input_buffer);
      for (int i = input_pos - 1; i < len; i++) {
        input_buffer[i] = input_buffer[i + 1];
      }
      input_pos--;

      // Move cursor back
      int col = shell_get_col();
      if (col > 2)
        shell_set_col(col - 1);

      // Redraw line
      redraw_line();
    }
  } else {
    // Add character to buffer
    if (strlen(input_buffer) < INPUT_BUFFER_SIZE - 1) {
      // Shift buffer right
      int len = strlen(input_buffer);
      for (int i = len; i > input_pos; i--) {
        input_buffer[i] = input_buffer[i - 1];
      }
      input_buffer[len + 1] = '\0'; // Ensure null termination

      // Insert character
      input_buffer[input_pos] = c;

      // Print character
      shell_putc(c, CONSOLE_COLOR_WHITE);
      input_pos++;

      // Redraw rest of line if we inserted in middle
      if (input_pos < len + 1) {
        redraw_line();
      }
    }
  }
}

// Handle arrow keys
void shell_handle_arrow(uint8_t scancode) {
  // If a dialog is active, any key dismisses it
  if (dialog_active) {
    dialog_active = 0;
    redraw_gui_terminal();
    shell_print("Welcome to project_OS Shell!\n", CONSOLE_COLOR_LIGHT_GREEN);
    shell_print("Type 'help' for available commands.\n\n", CONSOLE_COLOR_GRAY);
    show_prompt();
    return;
  }
  
  if (scancode == 0x4B) { // Left Arrow
    // Move cursor left visually and update input_pos (insertion point)
    if (input_pos > 0) {
      input_pos--;
      int col = shell_get_col();
      if (col > 2) { // Ensure we don't move past the prompt "$ "
        shell_set_col(col - 1);
      }
    }
  } else if (scancode == 0x4D) { // Right Arrow
    // Move cursor right visually and update input_pos (insertion point)
    // Only move right if we are not at the end of the current input string
    if (input_pos < strlen(input_buffer)) {
      input_pos++;
      shell_set_col(shell_get_col() + 1);
    }
  }
}

// Initialize shell
void shell_init(void) { 
  input_pos = 0;
  gui_mode = 0;
  cursor_visible = 0;
}

// Run shell
void shell_run(void) {
  // Initialize GUI terminal if in graphics mode
  if (is_graphics_mode() && graphics_is_available()) {
    draw_gui_terminal();
    gui_console_init();
  }
  
  shell_print("Welcome to project_OS Shell!\n", CONSOLE_COLOR_LIGHT_GREEN);
  shell_print("Type 'help' for available commands.\n\n", CONSOLE_COLOR_GRAY);

  show_prompt();
  
  // Show initial cursor
  if (gui_mode) {
    show_cursor();
  }

  // Shell runs in infinite loop - keyboard interrupt will call shell_putchar
  for (;;) {
    __asm__ volatile("hlt"); // Halt until next interrupt
  }
}
