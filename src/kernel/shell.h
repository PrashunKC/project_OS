#pragma once
#include "stdint.h"

// Reset shell input state, then run the interactive shell loop.
void shell_init(void);
void shell_run(void);

// Insert a translated keyboard character into the current command line.
void shell_putchar(char c);

// Handle supported set-1 arrow-key scancodes.
void shell_handle_arrow(uint8_t scancode);
