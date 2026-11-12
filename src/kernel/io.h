#pragma once
#include "stdint.h"

// Write one byte to an x86 I/O port.
void outb(uint16_t port, uint8_t val);