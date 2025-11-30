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
static const char scancode_to_ascii[] = {
    0,    0,    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-',  '=',
    '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[',  ']',
    '\n', 0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,    '*',
    0,    ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    '7',  '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0',  '.',
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0};

// Shift versions of characters
static const char scancode_to_ascii_shift[] = {
    0,    0,    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,    ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    '7',  '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.',
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0};

// Keyboard state
static uint8_t shift_pressed = 0;
static uint8_t caps_lock = 0;

// Keyboard interrupt handler
void keyboard_handler(Registers *regs) {
  (void)regs; // Unused parameter

  // Read scancode from keyboard
  uint8_t scancode = inb(KEYBOARD_DATA_PORT);

  // Check if it's a key release (bit 7 set)
  if (scancode & 0x80) {
    // Key release
    scancode &= 0x7F; // Remove release bit

    // Check for shift release
    if (scancode == 0x2A || scancode == 0x36) {
      shift_pressed = 0;
    }
  } else {
    // Key press

    // Check for special keys
    if (scancode == 0x2A || scancode == 0x36) {
      // Left or right shift
      shift_pressed = 1;
      return;
    } else if (scancode == 0x3A) {
      // Caps lock
      caps_lock = !caps_lock;
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
