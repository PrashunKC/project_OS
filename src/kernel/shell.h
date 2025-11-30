#pragma once
#include "stdint.h"

// Initialize and run the shell
void shell_init(void);
void shell_run(void);

// Handle character input from keyboard
void shell_putchar(char c);
