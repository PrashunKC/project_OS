#pragma once

#include "stdint.h"

#define _cdecl __attribute__((cdecl))

void putc(char c);
void puts(const char* str);
void _cdecl printf(const char* fmt, ...);
