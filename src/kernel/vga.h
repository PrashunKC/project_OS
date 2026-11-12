#pragma once
#include "stdint.h"

// Set the hardware cursor position in the 80x25 VGA text buffer.
void set_vga_cursor(int col, int row);