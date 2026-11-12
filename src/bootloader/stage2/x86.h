#pragma once

#include "stdint.h"

#define _cdecl __attribute__((cdecl))

// Write c to a BIOS text page using INT 10h teletype output.
void _cdecl x86_Video_WriteCharTeletype(char c, uint8_t page);

// Reset a BIOS disk device.
bool _cdecl x86_Disk_Reset(uint8_t drive);

// Read count sectors using BIOS CHS disk services.
bool _cdecl x86_Disk_Read(uint8_t drive, uint16_t cylinder, uint16_t sector, uint16_t head, uint8_t count, void* dataOut);

// Query BIOS disk geometry.
bool _cdecl x86_Disk_GetDriveParams(uint8_t drive, uint8_t* driveTypeOut, uint16_t* cylindersOut, uint16_t* sectorsOut, uint16_t* headsOut);

// Register image used to invoke a real-mode BIOS interrupt.
typedef struct {
    uint16_t ax, bx, cx, dx, si, di, es, ds, flags;
} Registers16;

// Invoke INT 10h with the supplied 16-bit register values.
void x86_Int10(Registers16* regs);
