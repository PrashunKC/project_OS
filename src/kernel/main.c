#include "stdint.h"

#define _cdecl __attribute__((cdecl))

// VGA text mode constants
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

// VGA color attributes
#define VGA_COLOR_BLACK        0x0
#define VGA_COLOR_BLUE         0x1
#define VGA_COLOR_GREEN        0x2
#define VGA_COLOR_CYAN         0x3
#define VGA_COLOR_RED          0x4
#define VGA_COLOR_MAGENTA      0x5
#define VGA_COLOR_BROWN        0x6
#define VGA_COLOR_LIGHT_GRAY   0x7
#define VGA_COLOR_DARK_GRAY    0x8
#define VGA_COLOR_LIGHT_BLUE   0x9
#define VGA_COLOR_LIGHT_GREEN  0xA
#define VGA_COLOR_LIGHT_CYAN   0xB
#define VGA_COLOR_LIGHT_RED    0xC
#define VGA_COLOR_LIGHT_MAGENTA 0xD
#define VGA_COLOR_YELLOW       0xE
#define VGA_COLOR_WHITE        0xF

// Create VGA color attribute byte
#define VGA_ENTRY_COLOR(fg, bg) ((bg << 4) | fg)

// Global cursor position
static int cursor_row = 0;
static int cursor_col = 0;
static uint8_t *video_memory = (uint8_t *)VGA_MEMORY;

// Clear the screen with specified color
void clear_screen(uint8_t color)
{
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2)
    {
        video_memory[i] = ' ';
        video_memory[i + 1] = color;
    }
    cursor_row = 0;
    cursor_col = 0;
}

// Print a character at current cursor position
void putchar_at(char c, uint8_t color, int col, int row)
{
    int offset = (row * VGA_WIDTH + col) * 2;
    video_memory[offset] = c;
    video_memory[offset + 1] = color;
}

// Print a character and advance cursor
void kputc(char c, uint8_t color)
{
    if (c == '\n')
    {
        cursor_col = 0;
        cursor_row++;
    }
    else if (c == '\r')
    {
        cursor_col = 0;
    }
    else
    {
        putchar_at(c, color, cursor_col, cursor_row);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH)
        {
            cursor_col = 0;
            cursor_row++;
        }
    }
    
    // Simple scroll handling
    if (cursor_row >= VGA_HEIGHT)
    {
        cursor_row = VGA_HEIGHT - 1;
    }
}

// Print a string with specified color
void kprint(const char *str, uint8_t color)
{
    while (*str)
    {
        kputc(*str, color);
        str++;
    }
}

// Print a newline
void knewline(void)
{
    cursor_col = 0;
    cursor_row++;
    if (cursor_row >= VGA_HEIGHT)
    {
        cursor_row = VGA_HEIGHT - 1;
    }
}

// Print a horizontal line
void kprint_line(char c, int count, uint8_t color)
{
    for (int i = 0; i < count; i++)
    {
        kputc(c, color);
    }
}

void _cdecl start(uint16_t bootDrive)
{
    // Color scheme for kernel messages
    uint8_t title_color = VGA_ENTRY_COLOR(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    uint8_t info_color = VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    uint8_t ok_color = VGA_ENTRY_COLOR(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    uint8_t header_color = VGA_ENTRY_COLOR(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    uint8_t detail_color = VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK);
    
    // Clear screen with black background
    clear_screen(VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK));
    
    // Print kernel header
    kprint_line('=', 50, header_color);
    knewline();
    kprint("          KERNEL STARTED SUCCESSFULLY", title_color);
    knewline();
    kprint_line('=', 50, header_color);
    knewline();
    knewline();
    
    // Print system information
    kprint("[INFO] Kernel loaded and running in 32-bit protected mode", info_color);
    knewline();
    kprint("[INFO] VGA text mode initialized (80x25)", info_color);
    knewline();
    kprint("[INFO] Video memory at 0xB8000", info_color);
    knewline();
    knewline();
    
    // Print status
    kprint("[OK] System initialization complete", ok_color);
    knewline();
    knewline();
    
    // Print details section
    kprint_line('-', 50, detail_color);
    knewline();
    kprint("System Details:", header_color);
    knewline();
    kprint("  - Architecture: x86 (i386)", detail_color);
    knewline();
    kprint("  - Mode: 32-bit Protected Mode", detail_color);
    knewline();
    kprint("  - Kernel Address: 0x100000 (1MB)", detail_color);
    knewline();
    kprint("  - Version: 1.0.0", detail_color);
    knewline();
    kprint("  - Build Date: 2025-11-30", detail_color);
    knewline();
    kprint_line('-', 50, detail_color);
    knewline();
    knewline();
    
    // Print final message
    kprint("Hello world from kernel!", VGA_ENTRY_COLOR(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    knewline();
    knewline();
    kprint("Kernel is now idle. Press Ctrl+Alt+Del to reboot.", detail_color);
    
    // Halt the CPU
    for (;;);
}
