/*
 * NBOS Graphics Header
 * 
 * BGI-compatible graphics library for NBOS.
 * Provides functions similar to Borland's graphics.h
 */

#ifndef _GRAPHICS_H
#define _GRAPHICS_H

#include "nbos.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

// Graphics drivers (for compatibility - NBOS auto-detects)
#define DETECT      0
#define VGA         9
#define VESA        10

// Colors (standard BGI palette)
#define BLACK           0
#define BLUE            1
#define GREEN           2
#define CYAN            3
#define RED             4
#define MAGENTA         5
#define BROWN           6
#define LIGHTGRAY       7
#define DARKGRAY        8
#define LIGHTBLUE       9
#define LIGHTGREEN      10
#define LIGHTCYAN       11
#define LIGHTRED        12
#define LIGHTMAGENTA    13
#define YELLOW          14
#define WHITE           15

// RGB color macro for true color modes
#define RGB(r, g, b)    (((r) << 16) | ((g) << 8) | (b))

// Fill patterns
#define EMPTY_FILL      0
#define SOLID_FILL      1
#define LINE_FILL       2
#define LTSLASH_FILL    3
#define SLASH_FILL      4
#define BKSLASH_FILL    5
#define LTBKSLASH_FILL  6
#define HATCH_FILL      7
#define XHATCH_FILL     8
#define INTERLEAVE_FILL 9
#define WIDE_DOT_FILL   10
#define CLOSE_DOT_FILL  11
#define USER_FILL       12

// Line styles
#define SOLID_LINE      0
#define DOTTED_LINE     1
#define CENTER_LINE     2
#define DASHED_LINE     3
#define USERBIT_LINE    4

// Text directions
#define HORIZ_DIR       0
#define VERT_DIR        1

// Text justification
#define LEFT_TEXT       0
#define CENTER_TEXT     1
#define RIGHT_TEXT      2
#define BOTTOM_TEXT     0
#define TOP_TEXT        2

/* ============================================================================
 * Graphics State
 * ============================================================================ */

// Internal state (managed by library)
static int _gfx_initialized = 0;
static int _current_color = WHITE;
static int _current_bkcolor = BLACK;
static int _fill_color = WHITE;
static int _fill_pattern = SOLID_FILL;
static int _screen_width = 0;
static int _screen_height = 0;

/* ============================================================================
 * Core Graphics Functions
 * ============================================================================ */

// Initialize graphics mode
static inline void initgraph(int *graphdriver, int *graphmode, const char *pathtodriver) {
    (void)graphdriver;
    (void)graphmode;
    (void)pathtodriver;
    
    // Get screen dimensions from kernel
    _screen_width = (int)syscall0(SYS_GETWIDTH);
    _screen_height = (int)syscall0(SYS_GETHEIGHT);
    
    _gfx_initialized = 1;
    _current_color = WHITE;
    _current_bkcolor = BLACK;
}

// Close graphics mode
static inline void closegraph(void) {
    _gfx_initialized = 0;
}

// Clear the screen
static inline void cleardevice(void) {
    syscall1(SYS_CLEAR, (uint64_t)_current_bkcolor);
}

// Get maximum X coordinate
static inline int getmaxx(void) {
    return _screen_width - 1;
}

// Get maximum Y coordinate
static inline int getmaxy(void) {
    return _screen_height - 1;
}

/* ============================================================================
 * Color Functions
 * ============================================================================ */

// Convert BGI color to RGB
static inline uint32_t _bgi_to_rgb(int color) {
    static const uint32_t palette[16] = {
        0x000000,  // BLACK
        0x0000AA,  // BLUE
        0x00AA00,  // GREEN
        0x00AAAA,  // CYAN
        0xAA0000,  // RED
        0xAA00AA,  // MAGENTA
        0xAA5500,  // BROWN
        0xAAAAAA,  // LIGHTGRAY
        0x555555,  // DARKGRAY
        0x5555FF,  // LIGHTBLUE
        0x55FF55,  // LIGHTGREEN
        0x55FFFF,  // LIGHTCYAN
        0xFF5555,  // LIGHTRED
        0xFF55FF,  // LIGHTMAGENTA
        0xFFFF55,  // YELLOW
        0xFFFFFF   // WHITE
    };
    if (color >= 0 && color < 16) {
        return palette[color];
    }
    return (uint32_t)color;  // Assume it's already RGB
}

// Set drawing color
static inline void setcolor(int color) {
    _current_color = color;
}

// Get current drawing color
static inline int getcolor(void) {
    return _current_color;
}

// Set background color
static inline void setbkcolor(int color) {
    _current_bkcolor = color;
}

// Get background color
static inline int getbkcolor(void) {
    return _current_bkcolor;
}

// Set fill style
static inline void setfillstyle(int pattern, int color) {
    _fill_pattern = pattern;
    _fill_color = color;
}

/* ============================================================================
 * Drawing Primitives
 * ============================================================================ */

// Put a pixel
static inline void putpixel(int x, int y, int color) {
    syscall3(SYS_PUTPIXEL, (uint64_t)x, (uint64_t)y, _bgi_to_rgb(color));
}

// Get pixel color
static inline int getpixel(int x, int y) {
    return (int)syscall2(SYS_GETPIXEL, (uint64_t)x, (uint64_t)y);
}

// Draw a line (Bresenham's algorithm)
static inline void line(int x1, int y1, int x2, int y2) {
    uint32_t color = _bgi_to_rgb(_current_color);
    int dx = x2 - x1;
    int dy = y2 - y1;
    int sx = (dx > 0) ? 1 : -1;
    int sy = (dy > 0) ? 1 : -1;
    dx = (dx < 0) ? -dx : dx;
    dy = (dy < 0) ? -dy : dy;
    
    int err = dx - dy;
    
    while (1) {
        syscall3(SYS_PUTPIXEL, (uint64_t)x1, (uint64_t)y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

// Draw a rectangle outline
static inline void rectangle(int left, int top, int right, int bottom) {
    line(left, top, right, top);
    line(right, top, right, bottom);
    line(right, bottom, left, bottom);
    line(left, bottom, left, top);
}

// Draw a filled rectangle (bar)
static inline void bar(int left, int top, int right, int bottom) {
    uint32_t color = _bgi_to_rgb(_fill_color);
    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            syscall3(SYS_PUTPIXEL, (uint64_t)x, (uint64_t)y, color);
        }
    }
}

// Draw a 3D bar (with outline)
static inline void bar3d(int left, int top, int right, int bottom, int depth, int topflag) {
    bar(left, top, right, bottom);
    rectangle(left, top, right, bottom);
    if (depth > 0) {
        line(right, top, right + depth, top - depth);
        line(right + depth, top - depth, right + depth, bottom - depth);
        line(right, bottom, right + depth, bottom - depth);
        if (topflag) {
            line(left, top, left + depth, top - depth);
            line(left + depth, top - depth, right + depth, top - depth);
        }
    }
}

// Draw a circle (midpoint algorithm)
static inline void circle(int xc, int yc, int radius) {
    uint32_t color = _bgi_to_rgb(_current_color);
    int x = radius;
    int y = 0;
    int err = 0;
    
    while (x >= y) {
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc + x), (uint64_t)(yc + y), color);
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc + y), (uint64_t)(yc + x), color);
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc - y), (uint64_t)(yc + x), color);
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc - x), (uint64_t)(yc + y), color);
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc - x), (uint64_t)(yc - y), color);
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc - y), (uint64_t)(yc - x), color);
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc + y), (uint64_t)(yc - x), color);
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc + x), (uint64_t)(yc - y), color);
        
        y++;
        err += 1 + 2*y;
        if (2*(err - x) + 1 > 0) {
            x--;
            err += 1 - 2*x;
        }
    }
}

// Draw a filled circle
static inline void fillcircle(int xc, int yc, int radius) {
    uint32_t color = _bgi_to_rgb(_fill_color);
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                syscall3(SYS_PUTPIXEL, (uint64_t)(xc + x), (uint64_t)(yc + y), color);
            }
        }
    }
}

// Draw an arc (simplified)
static inline void arc(int xc, int yc, int stangle, int endangle, int radius) {
    uint32_t color = _bgi_to_rgb(_current_color);
    // Simplified: draw full circle for now
    (void)stangle;
    (void)endangle;
    circle(xc, yc, radius);
    (void)color;
}

// Draw an ellipse
static inline void ellipse(int xc, int yc, int stangle, int endangle, int xradius, int yradius) {
    uint32_t color = _bgi_to_rgb(_current_color);
    (void)stangle;
    (void)endangle;
    
    // Midpoint ellipse algorithm
    int x = 0;
    int y = yradius;
    long rx2 = (long)xradius * xradius;
    long ry2 = (long)yradius * yradius;
    long px = 0;
    long py = 2 * rx2 * y;
    
    // Plot initial points
    syscall3(SYS_PUTPIXEL, (uint64_t)(xc + x), (uint64_t)(yc + y), color);
    syscall3(SYS_PUTPIXEL, (uint64_t)(xc - x), (uint64_t)(yc + y), color);
    syscall3(SYS_PUTPIXEL, (uint64_t)(xc + x), (uint64_t)(yc - y), color);
    syscall3(SYS_PUTPIXEL, (uint64_t)(xc - x), (uint64_t)(yc - y), color);
    
    // Region 1
    long p = ry2 - rx2 * yradius + rx2 / 4;
    while (px < py) {
        x++;
        px += 2 * ry2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= 2 * rx2;
            p += ry2 + px - py;
        }
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc + x), (uint64_t)(yc + y), color);
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc - x), (uint64_t)(yc + y), color);
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc + x), (uint64_t)(yc - y), color);
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc - x), (uint64_t)(yc - y), color);
    }
    
    // Region 2
    p = ry2 * (x + 1) * (x + 1) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;
    while (y > 0) {
        y--;
        py -= 2 * rx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += 2 * ry2;
            p += rx2 - py + px;
        }
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc + x), (uint64_t)(yc + y), color);
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc - x), (uint64_t)(yc + y), color);
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc + x), (uint64_t)(yc - y), color);
        syscall3(SYS_PUTPIXEL, (uint64_t)(xc - x), (uint64_t)(yc - y), color);
    }
}

/* ============================================================================
 * Text Functions
 * ============================================================================ */

// Output text at position
static inline void outtextxy(int x, int y, const char *text) {
    // For now, use simple character-by-character output
    // Each character is 8x16 pixels
    uint32_t fg = _bgi_to_rgb(_current_color);
    uint32_t bg = _bgi_to_rgb(_current_bkcolor);
    
    // System call to draw text
    syscall4(SYS_PRINT, (uint64_t)x, (uint64_t)y, (uint64_t)text, (fg << 32) | bg);
}

// Output text at current position
static inline void outtext(const char *text) {
    // Output at (0, 0) for now
    outtextxy(0, 0, text);
}

/* ============================================================================
 * Viewport and Clipping
 * ============================================================================ */

static int _vp_left = 0, _vp_top = 0, _vp_right = 0, _vp_bottom = 0;

static inline void setviewport(int left, int top, int right, int bottom, int clip) {
    (void)clip;
    _vp_left = left;
    _vp_top = top;
    _vp_right = right;
    _vp_bottom = bottom;
}

static inline void clearviewport(void) {
    bar(_vp_left, _vp_top, _vp_right, _vp_bottom);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

// Delay in milliseconds
static inline void delay(int ms) {
    sleep(ms);
}

// Check for keypress
static inline int kbhit_gfx(void) {
    return kbhit();
}

// Get character
static inline int getch(void) {
    return getkey();
}

#ifdef __cplusplus
}
#endif

#endif /* _GRAPHICS_H */
