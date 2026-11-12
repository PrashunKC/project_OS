#pragma once

#include "stdint.h"

#define _cdecl __attribute__((cdecl))

// Write one character through the BIOS teletype service.
void putc(char c);

// Write a null-terminated string.
void puts(const char* str);

// Print the supported stage2 subset of C printf formats.
void _cdecl printf(const char* fmt, ...);
