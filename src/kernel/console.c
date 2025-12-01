#include "console.h"
#include "graphics.h"

// Console state
static int cursor_row = 0;
static int cursor_col = 0;
static int console_cols = 0;
static int console_rows = 0;
static uint32_t bg_color = CONSOLE_COLOR_BLACK;

// Character cell size (from font)
#define CHAR_WIDTH 8
#define CHAR_HEIGHT 16

void console_init(void) {
    if (!graphics_is_available()) {
        return;
    }
    
    FramebufferInfo *fb = graphics_get_info();
    console_cols = fb->width / CHAR_WIDTH;
    console_rows = fb->height / CHAR_HEIGHT;
    cursor_row = 0;
    cursor_col = 0;
    bg_color = CONSOLE_COLOR_BLACK;
    
    // Clear screen
    graphics_clear(bg_color);
}

void console_clear(uint32_t color) {
    bg_color = color;
    graphics_clear(color);
    cursor_row = 0;
    cursor_col = 0;
}

void console_scroll(void) {
    if (!graphics_is_available()) {
        return;
    }
    
    FramebufferInfo *fb = graphics_get_info();
    volatile uint8_t *framebuffer = (volatile uint8_t *)(uintptr_t)fb->framebuffer_addr;
    int bytes_per_pixel = fb->bpp / 8;
    int line_height_bytes = CHAR_HEIGHT * fb->pitch;
    int screen_content_bytes = (console_rows - 1) * line_height_bytes;
    
    // Copy all lines up by one character row
    for (int i = 0; i < screen_content_bytes; i++) {
        framebuffer[i] = framebuffer[i + line_height_bytes];
    }
    
    // Clear the last line
    int last_line_start = (console_rows - 1) * CHAR_HEIGHT;
    for (int y = last_line_start; y < last_line_start + CHAR_HEIGHT; y++) {
        for (int x = 0; x < (int)fb->width; x++) {
            graphics_put_pixel(x, y, bg_color);
        }
    }
}

void console_putchar(char c, uint32_t color) {
    if (!graphics_is_available()) {
        return;
    }
    
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            // Clear the character
            int x = cursor_col * CHAR_WIDTH;
            int y = cursor_row * CHAR_HEIGHT;
            graphics_draw_char(x, y, ' ', bg_color, bg_color);
        }
    } else if (c == '\t') {
        // Tab: move to next multiple of 8
        cursor_col = (cursor_col + 8) & ~7;
        if (cursor_col >= console_cols) {
            cursor_col = 0;
            cursor_row++;
        }
    } else {
        // Regular character
        int x = cursor_col * CHAR_WIDTH;
        int y = cursor_row * CHAR_HEIGHT;
        graphics_draw_char(x, y, c, color, bg_color);
        cursor_col++;
        
        if (cursor_col >= console_cols) {
            cursor_col = 0;
            cursor_row++;
        }
    }
    
    // Handle scrolling
    if (cursor_row >= console_rows) {
        console_scroll();
        cursor_row = console_rows - 1;
    }
}

void console_print(const char *str, uint32_t color) {
    while (*str) {
        console_putchar(*str, color);
        str++;
    }
}

void console_newline(void) {
    cursor_col = 0;
    cursor_row++;
    
    if (cursor_row >= console_rows) {
        console_scroll();
        cursor_row = console_rows - 1;
    }
}

int console_get_row(void) {
    return cursor_row;
}

int console_get_col(void) {
    return cursor_col;
}

void console_set_row(int row) {
    if (row >= 0 && row < console_rows) {
        cursor_row = row;
    }
}

void console_set_col(int col) {
    if (col >= 0 && col < console_cols) {
        cursor_col = col;
    }
}

int console_get_width(void) {
    return console_cols;
}

int console_get_height(void) {
    return console_rows;
}
