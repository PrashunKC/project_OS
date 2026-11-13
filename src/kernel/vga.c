// VGA text-mode hardware cursor control.

#include "vga.h"
#include "stdint.h"
#include "io.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_CMD_PORT 0x3D4
#define VGA_DATA_PORT 0x3D5

static inline uint8_t vga_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void set_vga_cursor(int col, int row) {
    uint16_t pos = row * VGA_WIDTH + col;
    // Send high byte
    outb(VGA_CMD_PORT, 0x0E);
    outb(VGA_DATA_PORT, (pos >> 8) & 0xFF);
    // Send low byte
    outb(VGA_CMD_PORT, 0x0F);
    outb(VGA_DATA_PORT, pos & 0xFF);
}

int vga_is_available(void) {
    uint8_t misc_output = vga_inb(0x3CC);
    uint8_t input_status = vga_inb(0x3DA);

    return misc_output != 0xFF || input_status != 0xFF;
}
