#pragma once
#include "stdint.h"

// Keyboard I/O ports
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// Initialize keyboard driver
void keyboard_init(void);
