#pragma once
#include "stdint.h"
#include "disk.h"
#pragma pack(push, 1)

// On-disk FAT directory entry. The 11-byte Name is an 8.3 name without a dot.
typedef struct
{
    uint8_t Name[11];
    uint8_t Attributes;
    uint8_t _Reserved;
    uint8_t CreatedTimeTenths;
    uint16_t CreatedTime;
    uint16_t CreatedDate;
    uint16_t AccessedDate;
    uint16_t FirstClusterHigh;
    uint16_t ModifiedTime;
    uint16_t ModifiedDate;
    uint16_t FirstClusterLow;
    uint32_t Size;
}FAT_DirectoryEntry;

#pragma pack(pop)

typedef struct
{
    int Handle;
    bool IsDirectory;
    uint32_t Position;
    uint32_t Size;
}FAT_File;

// Attribute bits stored in FAT directory entries.
enum FAT_Attributes
{
    FAT_ATTRIBUTE_READ_ONLY = 0x01,
    FAT_ATTRIBUTE_HIDDEN = 0x02,
    FAT_ATTRIBUTE_SYSTEM = 0x04,
    FAT_ATTRIBUTE_VOLUME_ID = 0x08,
    FAT_ATTRIBUTE_DIRECTORY = 0x10,
    FAT_ATTRIBUTE_ARCHIVE = 0x20,
    FAT_ATTRIBUTE_LONG_NAME = FAT_ATTRIBUTE_READ_ONLY | FAT_ATTRIBUTE_HIDDEN | FAT_ATTRIBUTE_SYSTEM | FAT_ATTRIBUTE_VOLUME_ID
};

// Load the FAT boot sector, table, and root directory into stage2 memory.
bool FAT_Initialize(DISK* disk);

// Open a root-directory path and return a reusable file handle, or NULL.
FAT_File* FAT_Open(DISK* disk, const char* path);

// Read up to byteCCount bytes from the current file position.
uint32_t FAT_Read(DISK* disk, FAT_File* file, uint32_t byteCCount, void* dataOut);

// Read the next directory entry from an opened directory.
bool FAT_ReadEntry(DISK* disk, FAT_File* file, FAT_DirectoryEntry* dirEntry);

// Release a file handle allocated by FAT_Open.
void FAT_Close(FAT_File* file);

