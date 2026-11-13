#pragma once
#include "stdint.h"

// Set the hardware cursor position in the 80x25 VGA text buffer.
void set_vga_cursor(int col, int row);

// Probe whether the legacy VGA I/O ports respond.
int vga_is_available(void);