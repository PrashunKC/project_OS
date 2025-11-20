#include "stdint.h"
#include "stdio.h"
#include "disk.h"
#include "fat.h"
#include "string.h"
#include "memory.h"

#define _cdecl __attribute__((cdecl))

void _cdecl cstart_(uint16_t bootDrive)
{
    printf("\r\n\r\n");
    printf("**************************************************\r\n");
    printf("* BOOTLOADER STAGE 2 STARTED - GCC BUILD        *\r\n");
    printf("**************************************************\r\n");
    printf("\r\n");

    DISK disk;
    if (!DISK_Initialize(&disk, (uint8_t)bootDrive))
    {
        printf("Error: Could not initialize disk!\r\n");
        goto end;
    }

    printf("Disk initialized OK\r\n");

    if (!FAT_Initialize(&disk))
    {
        printf("Error: Could not initialize FAT!\r\n");
        goto end;
    }

    printf("FAT initialized OK\r\n");

    // Load Kernel
    FAT_File *fd = FAT_Open(&disk, "/kernel.bin");
    if (fd == NULL)
    {
        printf("Error: Could not open kernel.bin!\r\n");
        goto end;
    }

    printf("Loading kernel.bin (Size: %d bytes)...\r\n", fd->Size);

    // Read kernel to low memory buffer
    // We use 0x30000 as a temporary buffer.
    uint8_t *kernelBuffer = (uint8_t *)0x30000;
    uint32_t read = FAT_Read(&disk, fd, fd->Size, kernelBuffer);
    
    if (read != fd->Size)
    {
        printf("Error: Could not read entire kernel! Read %u of %u bytes.\r\n", read, fd->Size);
        goto end;
    }
    
    FAT_Close(fd);
    
    printf("Kernel loaded to temporary buffer. Copying to 0x100000...\r\n");
    
    // Copy to 1MB
    uint8_t *kernelStart = (uint8_t *)0x100000;
    memcpy(kernelStart, kernelBuffer, fd->Size);
    
    // Execute
    typedef void (*KernelStart)();
    KernelStart kernel = (KernelStart)kernelStart;
    kernel();

end:
    for (;;)
        ;
}
