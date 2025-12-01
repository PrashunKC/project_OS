#pragma once
#include "stdint.h"

// Keyboard I/O ports
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// Initialize keyboard driver
void keyboard_init(void);

// Check if a key is available in the buffer
int keyboard_has_key(void);

// Get the next key from the buffer (blocking)
char keyboard_get_key(void);

// Get the next key from the buffer (non-blocking, returns 0 if none)
char keyboard_get_key_nonblocking(void);
