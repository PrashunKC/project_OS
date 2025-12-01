# NBOS Software Development Kit

Welcome to the NBOS SDK! This toolkit allows you to write programs for NBOS from your Linux development machine.

## Requirements

- GCC cross-compiler for x86_64-elf (or regular gcc with -m64)
- NASM assembler
- Make

## Quick Start

```bash
# Set up the SDK environment
source sdk/env.sh

# Build an example
cd sdk/examples/hello
make

# The resulting .bin file can be loaded by NBOS
```

## Directory Structure

```
sdk/
├── include/          # NBOS headers
│   ├── nbos.h        # Main NBOS API
│   ├── graphics.h    # Graphics functions (BGI-compatible)
│   ├── stdint.h      # Integer types
│   └── string.h      # String functions
├── lib/              # Runtime libraries
│   └── crt0.o        # C runtime startup
├── examples/         # Example programs
│   ├── hello/        # Hello World
│   ├── graphics/     # Graphics demo
│   └── game/         # Simple game
├── tools/            # Build tools
│   ├── nbos-gcc      # Wrapper script for compiling
│   └── nbos-ld       # Wrapper script for linking
├── env.sh            # Environment setup script
└── README.md         # This file
```

## API Reference

### Graphics Functions (graphics.h)

```c
// Initialize graphics mode
void initgraph(int *gd, int *gm, const char *path);

// Close graphics mode  
void closegraph(void);

// Drawing primitives
void putpixel(int x, int y, int color);
void line(int x1, int y1, int x2, int y2);
void rectangle(int left, int top, int right, int bottom);
void circle(int x, int y, int radius);
void bar(int left, int top, int right, int bottom);

// Colors
void setcolor(int color);
void setbkcolor(int color);
void setfillstyle(int pattern, int color);

// Text
void outtextxy(int x, int y, const char *text);

// Screen info
int getmaxx(void);
int getmaxy(void);
```

### System Functions (nbos.h)

```c
// Memory
void *malloc(size_t size);
void free(void *ptr);

// I/O
void print(const char *str);
char getkey(void);
int kbhit(void);

// System
void exit(int code);
void sleep(int ms);
```

## Building Your Program

Create a simple program:

```c
// myprogram.c
#include <nbos.h>
#include <graphics.h>

int main(void) {
    int gd = DETECT, gm;
    initgraph(&gd, &gm, "");
    
    setcolor(WHITE);
    outtextxy(100, 100, "Hello from NBOS!");
    
    circle(320, 240, 50);
    
    getkey();  // Wait for keypress
    closegraph();
    return 0;
}
```

Compile it:

```bash
nbos-gcc -o myprogram.bin myprogram.c
```

## Colors

Standard BGI colors are supported:

| Value | Color        |
|-------|--------------|
| 0     | BLACK        |
| 1     | BLUE         |
| 2     | GREEN        |
| 3     | CYAN         |
| 4     | RED          |
| 5     | MAGENTA      |
| 6     | BROWN        |
| 7     | LIGHTGRAY    |
| 8     | DARKGRAY     |
| 9     | LIGHTBLUE    |
| 10    | LIGHTGREEN   |
| 11    | LIGHTCYAN    |
| 12    | LIGHTRED     |
| 13    | LIGHTMAGENTA |
| 14    | YELLOW       |
| 15    | WHITE        |

## License

MIT License - See LICENSE file
