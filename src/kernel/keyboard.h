#pragma once
#include "stdint.h"

// PS/2 controller ports used by the keyboard interrupt handler.
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// Register the IRQ1 handler and initialize keyboard state.
void keyboard_init(void);

// Return nonzero when the input queue contains a character.
int keyboard_has_key(void);

// Wait for and remove the next queued character.
char keyboard_get_key(void);

// Remove the next queued character, or return 0 when the queue is empty.
char keyboard_get_key_nonblocking(void);
