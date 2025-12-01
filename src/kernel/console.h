#pragma once
#include "stdint.h"

// Initialize the graphical console
void console_init(void);

// Print a character
void console_putchar(char c, uint32_t color);

// Print a string
void console_print(const char *str, uint32_t color);

// Print a newline
void console_newline(void);

// Clear the console
void console_clear(uint32_t bg_color);

// Get/set cursor position (in character cells)
int console_get_row(void);
int console_get_col(void);
void console_set_row(int row);
void console_set_col(int col);

// Get console dimensions
int console_get_width(void);  // columns
int console_get_height(void); // rows

// Scroll console up by one line
void console_scroll(void);

// Console colors
#define CONSOLE_COLOR_BLACK       0x000000
#define CONSOLE_COLOR_WHITE       0xFFFFFF
#define CONSOLE_COLOR_RED         0xFF0000
#define CONSOLE_COLOR_GREEN       0x00FF00
#define CONSOLE_COLOR_BLUE        0x0000FF
#define CONSOLE_COLOR_YELLOW      0xFFFF00
#define CONSOLE_COLOR_CYAN        0x00FFFF
#define CONSOLE_COLOR_MAGENTA     0xFF00FF
#define CONSOLE_COLOR_GRAY        0x808080
#define CONSOLE_COLOR_LIGHT_GRAY  0xC0C0C0
#define CONSOLE_COLOR_DARK_GRAY   0x404040
#define CONSOLE_COLOR_LIGHT_GREEN 0x90EE90
#define CONSOLE_COLOR_LIGHT_RED   0xFFA07A
#define CONSOLE_COLOR_ORANGE      0xFFA500
