#include "keyboard.h"
#include "isr.h"
#include "shell.h"
#include "stdint.h"

// External functions from main.c (for debugging only)
extern void kputc(char c, uint8_t color);

// Helper functions for I/O
static inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

// US QWERTY keyboard scancode set 1 layout
// Arrow keys (0x48=Up, 0x4B=Left, 0x4D=Right, 0x50=Down) are set to 0
static const char scancode_to_ascii[] = {
    0,    0,    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-',  '=',
    '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[',  ']',
    '\n', 0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,    '*',
    0,    ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,    0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,    '.',
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0};

// Shift versions of characters
// Arrow keys (0x48=Up, 0x4B=Left, 0x4D=Right, 0x50=Down) are set to 0
static const char scancode_to_ascii_shift[] = {
    0,    0,    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,    ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,    0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,   '.',
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0};

// Keyboard state
static uint8_t shift_pressed = 0;
static uint8_t caps_lock = 0;

// Keyboard interrupt handler
void keyboard_handler(Registers *regs) {
  (void)regs; // Unused parameter

  // Read scancode from keyboard
  uint8_t scancode = inb(KEYBOARD_DATA_PORT);
  static uint8_t extended = 0;

  // Handle extended scancode prefix (0xE0)
  if (scancode == 0xE0) {
    extended = 1;
    return;
  }

  // Check if it's a key release (bit 7 set)
  if (scancode & 0x80) {
    scancode &= 0x7F; // Remove release bit
    if (scancode == 0x2A || scancode == 0x36) {
      shift_pressed = 0;
    }
    extended = 0;
  } else {
    // Key press
    if (scancode == 0x2A || scancode == 0x36) {
      shift_pressed = 1;
      extended = 0;
      return;
    } else if (scancode == 0x3A) {
      caps_lock = !caps_lock;
      extended = 0;
      return;
    }

    // Arrow keys (extended scancodes: Up=0x48, Down=0x50, Left=0x4B, Right=0x4D)
    // Also block non-extended arrow scancodes (same codes used by numpad without NumLock)
    if (scancode == 0x48 || scancode == 0x50 || scancode == 0x4B || scancode == 0x4D) {
      // Ignore arrow keys for now
      extended = 0;
      return;
    }

    if (extended) {
      // Other extended keys - ignore for now
      extended = 0;
      return;
    }

    // Translate scancode to ASCII
    char c = 0;
    if (scancode < sizeof(scancode_to_ascii)) {
      if (shift_pressed) {
        c = scancode_to_ascii_shift[scancode];
      } else {
        c = scancode_to_ascii[scancode];
      }

      // Apply caps lock to letters
      if (caps_lock && c >= 'a' && c <= 'z') {
        c = c - 'a' + 'A';
      } else if (caps_lock && c >= 'A' && c <= 'Z') {
        c = c - 'A' + 'a';
      }

      // Print character if valid
      if (c != 0) {
        shell_putchar(c);
      }
    }
  }
}

void keyboard_init(void) {
  // Register keyboard interrupt handler (IRQ 1 = interrupt 33)
  register_interrupt_handler(33, keyboard_handler);
}
