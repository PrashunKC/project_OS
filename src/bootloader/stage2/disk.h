#pragma once

#include "stdint.h"

// BIOS disk geometry used to convert logical block addresses to CHS values.
typedef struct
{
    uint8_t id;
    uint16_t cylinders;
    uint16_t sectors;
    uint16_t heads;
} DISK;

// Query BIOS geometry for driveNumber and initialize disk.
bool DISK_Initialize(DISK* disk, uint8_t driveNumber);

// Read sectors beginning at lba into dataOut, retrying failed BIOS reads.
bool DISK_ReadSectors(DISK* disk, uint32_t lba, uint8_t sectors, void* dataOut);
