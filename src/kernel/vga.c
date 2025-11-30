
#include "vga.h"
#include "stdint.h"
#include "io.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_CMD_PORT 0x3D4
#define VGA_DATA_PORT 0x3D5

void set_vga_cursor(int col, int row) {
    uint16_t pos = row * VGA_WIDTH + col;
    // Send high byte
    outb(VGA_CMD_PORT, 0x0E);
    outb(VGA_DATA_PORT, (pos >> 8) & 0xFF);
    // Send low byte
    outb(VGA_CMD_PORT, 0x0F);
    outb(VGA_DATA_PORT, pos & 0xFF);
}
