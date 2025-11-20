#include "disk.h"
#include "x86.h"

// Helper function for 32-bit division
static uint32_t div32(uint32_t dividend, uint16_t divisor, uint32_t* remainder)
{
    if(remainder)
        *remainder = dividend % divisor;
    return dividend / divisor;
}

bool DISK_Initialize(DISK* disk, uint8_t driveNumber)
{
    uint8_t driveType;
    uint16_t cylinders, sectors, heads;

    disk->id = driveNumber;

    if(!x86_Disk_GetDriveParams(disk->id, &driveType, &cylinders, &sectors, &heads)) // lemme see...
    {
        return false;
    }

    disk->cylinders = cylinders;
    disk->heads = heads;
    disk->sectors = sectors;

    return true;
}

void DISK_LBA2CHS(DISK* disk, uint32_t lba, uint16_t* cylinderOut, uint16_t* sectorOut, uint16_t* headOut)
{
    //basic maths for conversion.
    uint32_t temp, rem;
    
    // sector = (lba % sectors_per_track) + 1
    temp = div32(lba, disk->sectors, &rem);
    *sectorOut = (uint16_t)rem + 1;
    
    // cylinder = temp / heads
    // head = temp % heads
    *cylinderOut = (uint16_t)div32(temp, disk->heads, &rem);
    *headOut = (uint16_t)rem;
}


bool DISK_ReadSectors(DISK* disk, uint32_t lba, uint8_t sectors, void* dataOut)
{
    uint16_t cylinder, sector, head;
    DISK_LBA2CHS(disk, lba, &cylinder, &sector, &head);

    // loop
    for(int i = 0; i < 3; i++)
    {
        if(x86_Disk_Read(disk->id, cylinder, sector, head, sectors, dataOut))
        {
            return true;
        }
        x86_Disk_Reset(disk->id);
    }

    return false;
}
