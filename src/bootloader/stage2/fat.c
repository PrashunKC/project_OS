#include "fat.h"
#include "stdio.h"
#include "memdefs.h"
#include "utility.h"
#include "string.h"
#include "memory.h"
#include "ctype.h"

#define SECTOR_SIZE 512
#define MAX_PATH_SIZE 512
#define MAX_FILE_HANDLES 10
#define ROOT_DIRECTORY_HANDLE -1

#pragma pack(push, 1)
typedef struct
{
    uint8_t BootJumpInstruction[3];
    uint8_t OemIdentifier[8];
    uint16_t BytesPerSector;
    uint8_t SectorsPerCluster;
    uint16_t ReservedSectors;
    uint8_t FatCount;
    uint16_t DirEntryCount;
    uint16_t TotalSectors;
    uint8_t MediaDescriptorType;
    uint16_t SectorsPerFat;
    uint16_t SectorsPerTrack;
    uint16_t Heads;
    uint32_t HiddenSectors;
    uint32_t LargeSectorCount;

    uint8_t DriveNumber;
    uint8_t _Reserved;
    uint8_t Signature;
    uint8_t VolumeLabel[11];
    uint8_t SystemId[8];

} FAT_BootSector;

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
} DirectoryEntry;

#pragma pack(pop)

typedef struct
{
    FAT_File Public;
    bool Opened;
    uint32_t FirstCluster;
    uint32_t CurrentCluster;
    uint32_t CurrentSectorInCluster;
    uint8_t Buffer[SECTOR_SIZE];
} FAT_FileData;

typedef struct
{
    union
    {
        FAT_BootSector BootSector;
        uint8_t BootSectorBytes[SECTOR_SIZE];
    } BS;

    FAT_FileData RootDirectory;

    FAT_FileData OpenedFiles[MAX_FILE_HANDLES];

} FAT_Data;

static FAT_Data *g_Data;
static uint8_t *g_Fat = NULL;
static uint32_t g_DataSectionLba;

// Forward declarations
bool FAT_ReadFat(DISK* disk);
bool FAT_ReadRootDirectory(DISK* disk);

// Helper function
uint32_t min(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

bool FAT_readBootSector(DISK *disk)
{
    return DISK_ReadSectors(disk, 0, 1, g_Data->BS.BootSectorBytes);
}

bool FAT_ReadFat(DISK *disk)
{
    return DISK_ReadSectors(disk, g_Data->BS.BootSector.ReservedSectors, g_Data->BS.BootSector.SectorsPerFat, g_Fat);
}

bool FAT_ReadRootDirectory(DISK *disk)
{
    uint32_t lba = g_Data->BS.BootSector.ReservedSectors + g_Data->BS.BootSector.SectorsPerFat * g_Data->BS.BootSector.FatCount;
    uint32_t size = sizeof(FAT_DirectoryEntry) * g_Data->BS.BootSector.DirEntryCount;
    uint32_t sectors = (size + g_Data->BS.BootSector.BytesPerSector - 1) / g_Data->BS.BootSector.BytesPerSector;
    
    uint8_t *rootDirBuffer = (uint8_t *)g_Fat + g_Data->BS.BootSector.SectorsPerFat * g_Data->BS.BootSector.BytesPerSector;
    return DISK_ReadSectors(disk, lba, sectors, rootDirBuffer);
}

bool FAT_Initialize(DISK *disk)
{
    // read boot sector
    g_Data = (FAT_Data *)MEMORY_FAT_ADDR;
    if (!FAT_readBootSector(disk))
    {
        printf("Error: Could not read boot sector!\r\n");
        return false;
    }

    // read fat
    g_Fat = (uint8_t *)g_Data + sizeof(FAT_Data);
    uint32_t fatSize = g_Data->BS.BootSector.SectorsPerFat * g_Data->BS.BootSector.BytesPerSector;
    if (sizeof(FAT_Data) + fatSize > MEMORY_FAT_SIZE)
    {
        printf("Error: FAT data size exceeds allocated memory! Required: %u, Available: %u\r\n", sizeof(FAT_Data) + fatSize, MEMORY_FAT_SIZE);
        return false;
    }

    if (!FAT_ReadFat(disk))
    {
        printf("Error: Could not read FAT!\r\n");
        return false;
    }

    // read root directory
    uint32_t rootDirLba = g_Data->BS.BootSector.ReservedSectors + g_Data->BS.BootSector.SectorsPerFat * g_Data->BS.BootSector.FatCount;
    uint32_t rootDirSize = sizeof(FAT_DirectoryEntry) * g_Data->BS.BootSector.DirEntryCount;
    rootDirSize = align(rootDirSize, g_Data->BS.BootSector.BytesPerSector);

    if (sizeof(FAT_Data) + fatSize + rootDirSize > MEMORY_FAT_SIZE)
    {
        printf("Error: FAT root directory size exceeds allocated memory! Required: %u, Available: %u\r\n", sizeof(FAT_Data) + fatSize + rootDirSize, MEMORY_FAT_SIZE);
        return false;
    }

    if (!FAT_ReadRootDirectory(disk))
    {
        printf("Error: Could not read FAT root directory!\r\n");
        return false;
    }

    // open root directory file
    g_Data->RootDirectory.Public.Handle = ROOT_DIRECTORY_HANDLE;
    g_Data->RootDirectory.Public.IsDirectory = true;
    g_Data->RootDirectory.Public.Position = 0;
    g_Data->RootDirectory.Public.Size = sizeof(FAT_DirectoryEntry) * g_Data->BS.BootSector.DirEntryCount;
    g_Data->RootDirectory.Opened = true;
    g_Data->RootDirectory.FirstCluster = 0;
    g_Data->RootDirectory.CurrentCluster = 0;
    g_Data->RootDirectory.CurrentSectorInCluster = 0;

    if (!DISK_ReadSectors(disk, rootDirLba, 1, g_Data->RootDirectory.Buffer))
    {
        printf("Error: Could not read root directory!\r\n");
        return false;
    }

    uint32_t rootDirSectors = (rootDirSize + g_Data->BS.BootSector.BytesPerSector - 1) / g_Data->BS.BootSector.BytesPerSector;
    g_DataSectionLba = rootDirLba + rootDirSectors;

    // reset opened files
    for (int i = 0; i < MAX_FILE_HANDLES; i++)
    {
        g_Data->OpenedFiles[i].Opened = false;
    }

    return true;
}

uint32_t FAT_ClusterToLba(uint32_t cluster)
{
    return g_DataSectionLba + (cluster - 2) * g_Data->BS.BootSector.SectorsPerCluster;
}

FAT_File *FAT_OpenEntry(DISK *disk, FAT_DirectoryEntry *entry)
{
    // Find a free file handle
    int handle = -1;
    for (int i = 0; i < MAX_FILE_HANDLES && handle < 0; i++)
    {
        if (!g_Data->OpenedFiles[i].Opened)
        {
            handle = i;
        }
    }

    if (handle == -1)
    {
        printf("Error: No free file handles available!\r\n");
        return NULL;
    }

    FAT_FileData *fd = &g_Data->OpenedFiles[handle];
    fd->Public.Handle = handle;
    fd->Public.IsDirectory = (entry->Attributes & FAT_ATTRIBUTE_DIRECTORY) != 0;
    fd->Public.Position = 0;
    fd->Public.Size = entry->Size;
    fd->Opened = true;
    fd->FirstCluster = (entry->FirstClusterHigh << 16) | entry->FirstClusterLow;
    fd->CurrentCluster = fd->FirstCluster;
    fd->CurrentSectorInCluster = 0;

    uint32_t lba = FAT_ClusterToLba(fd->CurrentCluster);

    // Load the first sector of the file/directory
    if (!DISK_ReadSectors(disk, lba, 1, fd->Buffer))
    {
        printf("Error: Could not read first sector of the file!\r\n");
        return NULL;
    }
    
    fd->Opened = true;
    return &fd->Public;
}

uint32_t FAT_NextCluster(uint32_t currentCluster)
{
    uint32_t fatIndex = currentCluster * 3 / 2;

    if (currentCluster % 2 == 0)
    {
        return (*(uint16_t *)(g_Fat + fatIndex)) & 0x0FFF;
    }

    else
    {
        return (*(uint16_t *)(g_Fat + fatIndex)) >> 4;
    }
}

uint32_t FAT_Read(DISK *disk, FAT_File *file, uint32_t byteCCount, void *dataOut)
{
    FAT_FileData *fd = (file->Handle == ROOT_DIRECTORY_HANDLE)
                               ? &g_Data->RootDirectory
                               : &g_Data->OpenedFiles[file->Handle];

    uint8_t *u8DataOut = (uint8_t *)dataOut;
    uint32_t byteCount = min(byteCCount, fd->Public.Size - fd->Public.Position);

    while (byteCount > 0)
    {
        uint32_t leftInBuffer = SECTOR_SIZE - (fd->Public.Position % SECTOR_SIZE);
        uint32_t take = min(byteCount, leftInBuffer);

        memcpy(u8DataOut, fd->Buffer + fd->Public.Position % SECTOR_SIZE, take);
        u8DataOut += take;
        fd->Public.Position += take;
        byteCount -= take;

        if (byteCount > 0)
        {
            if (fd->Public.Handle == ROOT_DIRECTORY_HANDLE)
            {
                ++fd->CurrentCluster;

                if (!DISK_ReadSectors(disk, fd->CurrentCluster, 1, fd->Buffer))
                {
                    printf("Error: Could not read sector from disk!\r\n");
                    return u8DataOut - (uint8_t *)dataOut;
                }
            }

            else
            {

                if (++fd->CurrentSectorInCluster >= g_Data->BS.BootSector.SectorsPerCluster)
                {
                    fd->CurrentSectorInCluster = 0;
                    fd->CurrentCluster = FAT_NextCluster(fd->CurrentCluster);
                }

                if (fd->CurrentCluster >= 0x0FF8)
                {
                    // End of file/cluster chain reached
                    printf("Error: Unexpected end of cluster chain!\r\n");
                    break;
                }

                if (!DISK_ReadSectors(disk, FAT_ClusterToLba(fd->CurrentCluster) + fd->CurrentSectorInCluster, 1, fd->Buffer))
                {
                    printf("Error: Could not read sector from disk!\r\n");
                    return u8DataOut - (uint8_t *)dataOut;
                }
            }
        }
    }

    return u8DataOut - (uint8_t *)dataOut;
}

bool FAT_ReadEntry(DISK *disk, FAT_File *file, FAT_DirectoryEntry *dirEntry)
{
    return FAT_Read(disk, file, sizeof(FAT_DirectoryEntry), dirEntry) == sizeof(FAT_DirectoryEntry);
}

void FAT_Close(FAT_File *file)
{
    if (file == NULL)
    {
        return;
    }

    if (file->Handle == ROOT_DIRECTORY_HANDLE)
    {
        g_Data->RootDirectory.Public.Position = 0;
        g_Data->RootDirectory.CurrentCluster = g_Data->RootDirectory.FirstCluster;
        g_Data->RootDirectory.CurrentSectorInCluster = 0;
    }
    else if (file->Handle >= 0 && file->Handle < MAX_FILE_HANDLES)
    {
        g_Data->OpenedFiles[file->Handle].Opened = false;
    }
}

bool FAT_FindFile(DISK* disk, FAT_File* file, const char* name, FAT_DirectoryEntry* entryOut)
{
    char fatName[11];

    memset(fatName, ' ', sizeof(fatName));
    const char* ext = strchr(name, '.');

    if(ext == NULL)
    {
        ext = name + 11;
    }

    for(int i = 0; i < 8 && name + i < ext; i++)
    {
        fatName[i] = toupper(name[i]);
    }

    if(ext != NULL)
    {
        for(int i = 0; i < 3 && ext + 1 + i < name + strlen(name); i++)
        {
            fatName[8 + i] = toupper(ext[1 + i]);
        }
    }

    FAT_DirectoryEntry entry;
    
    while(FAT_ReadEntry(disk, file, &entry))
    {
        if(memcmp(fatName, entry.Name, 11) == 0)
        {
            *entryOut = entry;
            return true;
        }
    }

    return false;
}

FAT_File *FAT_Open(DISK *disk, const char *path)
{
    char buffer[MAX_PATH_SIZE];

    if (path[0] == '/')
    {
        path++;
    }

    FAT_File *current = &g_Data->RootDirectory.Public;

    while (*path)
    {
        // extract file name from path
        bool isLast = false;
        const char *delim = strchr(path, '/');
        if (delim != NULL)
        {
            memcpy(buffer, path, delim - path);
            buffer[delim - path] = '\0';
            path = delim + 1;
        }

        else
        {
            unsigned len = strlen(path);
            memcpy(buffer, path, len);
            buffer[len] = '\0';
            path += len;
            isLast = true;
        }

        FAT_DirectoryEntry entry;
        if (FAT_FindFile(disk, current, buffer, &entry))
        {
            FAT_Close(current);
            if (!isLast && (entry.Attributes & FAT_ATTRIBUTE_DIRECTORY) == 0)
            {
                printf("Error: '%s' is not a directory!\r\n", buffer);
                return NULL;
            }

            current = FAT_OpenEntry(disk, &entry);
        }

        else
        {
            FAT_Close(current);
            printf("Error: File '%s' not found!\r\n", buffer);
            return NULL;
        }
    }

    return current;
}
