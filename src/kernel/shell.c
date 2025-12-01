#include "shell.h"
#include "paging.h"
#include "stdint.h"

// External functions from main.c
extern void kputc(char c, uint8_t color);
extern void kprint(const char *str, uint8_t color);
extern void knewline(void);
extern void clear_screen(uint8_t color);
extern int get_cursor_col(void);
extern int get_cursor_row(void);
extern void set_cursor_col(int col);
extern void set_cursor_row(int row);

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
  kprint("$ ", VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
}

// Built-in command: help
static void cmd_help(void) {
  knewline();
  kprint("Available commands:",
         VGA_ENTRY_COLOR(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
  knewline();
  kprint("  help   - Show this help message",
         VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  knewline();
  kprint("  clear  - Clear the screen",
         VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  knewline();
  kprint("  echo   - Print text to screen",
         VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  knewline();
  kprint("  peek   - Inspect memory (peek <addr>)",
         VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  knewline();
  kprint("  reboot - Reboot the system",
         VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  knewline();
  kprint("  shutdown - Power off the system",
         VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  knewline();
  kprint("  halt   - Halt the system",
         VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  knewline();
  kprint("  about  - Show OS information",
         VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  knewline();
  kprint("  info   - Show system register info",
         VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  knewline();
}

// Built-in command: clear
static void cmd_clear(void) {
  clear_screen(VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
}

// Built-in command: echo
static void cmd_echo(const char *args) {
  knewline();
  kprint(args, VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  knewline();
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
  kputc(hex[(byte >> 4) & 0xF],
        VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  kputc(hex[byte & 0xF], VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

// Built-in command: reboot
static void cmd_reboot(void) {
  knewline();
  kprint("Rebooting...", VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));

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
  knewline();
  kprint("Shutting down...",
         VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));

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
  knewline();
  kprint("Shutdown failed (ACPI not implemented).",
         VGA_ENTRY_COLOR(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
  knewline();
  kprint("System Halted.",
         VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));

  // Disable interrupts and halt
  __asm__ volatile("cli");
  for (;;) {
    __asm__ volatile("hlt");
  }
}

// Built-in command: halt
static void cmd_halt(void) {
  knewline();
  kprint("System Halted.",
         VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
  kprint(" Press Reset to restart.",
         VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));

  // Disable interrupts and halt
  __asm__ volatile("cli");
  for (;;) {
    __asm__ volatile("hlt");
  }
}

// Built-in command: peek
static void cmd_peek(const char *args) {
  if (!*args) {
    knewline();
    kprint("Usage: peek <address>",
           VGA_ENTRY_COLOR(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
    knewline();
    return;
  }

  uint64_t addr = atoi_hex(args);
  uint8_t *ptr = (uint8_t *)(uintptr_t)addr;

  knewline();
  kprint("Memory at 0x",
         VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  kprint(args, VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  kprint(": ", VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));

  // Print 16 bytes
  for (int i = 0; i < 16; i++) {
    print_hex_byte(ptr[i]);
    kputc(' ', VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  }
  knewline();
}

// Built-in command: about
static void cmd_about(void) {
  knewline();
  kprint("project_OS - A Custom Operating System",
         VGA_ENTRY_COLOR(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
  knewline();
  kprint("Version: 1.0.0", VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  knewline();
  kprint("Features:", VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  knewline();
  kprint("  - 32-bit Protected Mode",
         VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  knewline();
  kprint("  - Interrupt Handling (IDT)",
         VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  knewline();
  kprint("  - PS/2 Keyboard Driver",
         VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  knewline();
  kprint("  - Interactive Shell",
         VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  knewline();
}

// Helper to print 32-bit hex value
static void print_hex32(uint32_t val) {
  char hex[] = "0123456789ABCDEF";
  kprint("0x", VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  for (int i = 28; i >= 0; i -= 4) {
    kputc(hex[(val >> i) & 0xF],
          VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  }
}

// Built-in command: info
static void cmd_info(void) {
  knewline();
  kprint("System Information:",
         VGA_ENTRY_COLOR(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
  knewline();

  kprint("  CR0: ", VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  print_hex32(get_cr0());
  kprint("  (PG=", VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  kprint((get_cr0() & 0x80000000) ? "1" : "0",
         VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  kprint(")", VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  knewline();

  kprint("  CR3: ", VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  print_hex32(get_cr3());
  knewline();

  kprint("  CR4: ", VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  print_hex32(get_cr4());
  kprint("  (PAE=", VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  kprint((get_cr4() & 0x20) ? "1" : "0",
         VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  kprint(")", VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  knewline();
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
    knewline();
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
  } else if (strcmp(cmd, "info") == 0) { // Added info command dispatch
    cmd_info();
  } else {
    knewline();
    kprint("Unknown command: ",
           VGA_ENTRY_COLOR(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
    kprint(cmd, VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    knewline();
    kprint("Type 'help' for available commands",
           VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
    knewline();
  }

  // Reset input buffer
  input_pos = 0;
  input_buffer[0] = '\0'; // Clear buffer
  show_prompt();
}

// Helper to redraw line from current position
static void redraw_line(void) {
  int start_col = get_cursor_col();
  int start_row = get_cursor_row();

  // Print remaining part of buffer
  kprint(&input_buffer[input_pos],
         VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

  // Clear extra character at the end (if we deleted)
  kputc(' ', VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

  // Restore cursor
  set_cursor_row(start_row);
  set_cursor_col(start_col);
}

// Handle character input
void shell_putchar(char c) {
  if (c == '\n') {
    // Move cursor to end of line before executing
    int len = strlen(input_buffer);
    if (input_pos < len) {
      // We need to calculate where the end is visually
      // This is tricky if we wrapped lines, but assuming single line for now
      // Simple hack: just print newline
    }
    execute_command();
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
      int col = get_cursor_col();
      if (col > 2)
        set_cursor_col(col - 1);

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
      kputc(c, VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
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
  if (scancode == 0x4B) { // Left Arrow
    // Move cursor left visually and update input_pos (insertion point)
    if (input_pos > 0) {
      input_pos--;
      int col = get_cursor_col();
      if (col > 2) { // Ensure we don't move past the prompt "$ "
        set_cursor_col(col - 1);
      }
    }
  } else if (scancode == 0x4D) { // Right Arrow
    // Move cursor right visually and update input_pos (insertion point)
    // Only move right if we are not at the end of the current input string
    if (input_pos < strlen(input_buffer)) {
      input_pos++;
      set_cursor_col(get_cursor_col() + 1);
    }
  }
}

// Initialize shell
void shell_init(void) { input_pos = 0; }

// Run shell
void shell_run(void) {
  knewline();
  kprint("Welcome to project_OS Shell!",
         VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
  knewline();
  kprint("Type 'help' for available commands.",
         VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
  knewline();
  knewline();

  show_prompt();

  // Shell runs in infinite loop - keyboard interrupt will call shell_putchar
  for (;;) {
    __asm__ volatile("hlt"); // Halt until next interrupt
  }
}
