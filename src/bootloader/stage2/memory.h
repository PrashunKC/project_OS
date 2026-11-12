#pragma once

#include "stdint.h"

// Copy num bytes from src to dst. Regions must not overlap.
void* memcpy(void* dst, const void* src, uint16_t num);

// Fill num bytes at ptr with the low byte of value.
void* memset(void* ptr, int value, uint16_t num);

// Compare num bytes and return the first unsigned-byte difference.
int memcmp(const void* ptr1, const void* ptr2, uint16_t num);

