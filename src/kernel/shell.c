#include "shell.h"
#include "stdint.h"

// External functions from main.c
extern void kputc(char c, uint8_t color);
extern void kprint(const char *str, uint8_t color);
extern void knewline(void);
extern void clear_screen(uint8_t color);
extern int get_cursor_col(void);

// VGA colors
#define VGA_COLOR_BLACK 0x0
#define VGA_COLOR_LIGHT_GRAY 0x7
#define VGA_COLOR_LIGHT_GREEN 0xA
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
  kprint("  about  - Show OS information",
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

// Execute command
static void execute_command(void) {
  // Null-terminate input
  input_buffer[input_pos] = '\0';

  // Skip empty commands
  if (input_pos == 0) {
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
  } else if (strcmp(cmd, "about") == 0) {
    cmd_about();
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
  show_prompt();
}

// Handle character input
void shell_putchar(char c) {
  if (c == '\n') {
    // Execute command on Enter
    execute_command();
  } else if (c == '\b') {
    // Handle backspace
    if (input_pos > 0) {
      input_pos--;
      kputc('\b', VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    }
  } else {
    // Add character to buffer
    if (input_pos < INPUT_BUFFER_SIZE - 1) {
      input_buffer[input_pos++] = c;
      kputc(c, VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
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
