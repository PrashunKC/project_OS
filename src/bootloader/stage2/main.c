#include "stdint.h"
#include "stdio.h"
#include "disk.h"
#include "fat.h"
#include "string.h"
#include "memory.h"

#define _cdecl __attribute__((cdecl))

// Log level definitions for enhanced debugging
#define LOG_INFO    "[INFO] "
#define LOG_OK      "[OK]   "
#define LOG_ERROR   "[ERR]  "
#define LOG_DEBUG   "[DBG]  "

void _cdecl cstart_(uint16_t bootDrive)
{
    printf("\r\n\r\n");
    printf("**************************************************\r\n");
    printf("* BOOTLOADER STAGE 2 STARTED - GCC BUILD        *\r\n");
    printf("* Version: 1.0.0 | Date: 2025-11-30             *\r\n");
    printf("**************************************************\r\n");
    printf("\r\n");

    printf(LOG_INFO "Boot drive: 0x%x\r\n", bootDrive);
    printf(LOG_INFO "Initializing system components...\r\n");
    printf("\r\n");

    // Step 1: Initialize disk subsystem
    printf(LOG_INFO "Step 1: Initializing disk subsystem...\r\n");
    DISK disk;
    if (!DISK_Initialize(&disk, (uint8_t)bootDrive))
    {
        printf(LOG_ERROR "Could not initialize disk!\r\n");
        printf(LOG_ERROR "Boot halted. Please check disk connection.\r\n");
        goto end;
    }
    printf(LOG_OK "Disk initialized successfully\r\n");
    printf(LOG_DEBUG "Disk ID: 0x%x, Cylinders: %u, Heads: %u, Sectors: %u\r\n", 
           disk.id, disk.cylinders, disk.heads, disk.sectors);
    printf("\r\n");

    // Step 2: Initialize FAT filesystem
    printf(LOG_INFO "Step 2: Initializing FAT filesystem...\r\n");
    if (!FAT_Initialize(&disk))
    {
        printf(LOG_ERROR "Could not initialize FAT filesystem!\r\n");
        printf(LOG_ERROR "Boot halted. Please verify disk format.\r\n");
        goto end;
    }
    printf(LOG_OK "FAT filesystem initialized successfully\r\n");
    printf("\r\n");

    // Step 3: Load Kernel
    printf(LOG_INFO "Step 3: Loading kernel...\r\n");
    printf(LOG_INFO "Searching for kernel.bin in root directory...\r\n");
    FAT_File *fd = FAT_Open(&disk, "/kernel.bin");
    if (fd == NULL)
    {
        printf(LOG_ERROR "Could not open kernel.bin!\r\n");
        printf(LOG_ERROR "Boot halted. Kernel file not found.\r\n");
        goto end;
    }
    printf(LOG_OK "Kernel file found\r\n");
    printf(LOG_DEBUG "Kernel size: %u bytes\r\n", fd->Size);
    printf("\r\n");

    // Step 4: Read kernel to temporary buffer
    printf(LOG_INFO "Step 4: Reading kernel to temporary buffer (0x30000)...\r\n");
    uint8_t *kernelBuffer = (uint8_t *)0x30000;
    uint32_t read = FAT_Read(&disk, fd, fd->Size, kernelBuffer);
    
    if (read != fd->Size)
    {
        printf(LOG_ERROR "Could not read entire kernel!\r\n");
        printf(LOG_ERROR "Expected: %u bytes, Read: %u bytes\r\n", fd->Size, read);
        printf(LOG_ERROR "Boot halted. Disk read error.\r\n");
        goto end;
    }
    printf(LOG_OK "Kernel read successfully (%u bytes)\r\n", read);
    
    FAT_Close(fd);
    printf("\r\n");
    
    // Step 5: Copy kernel to final location
    printf(LOG_INFO "Step 5: Copying kernel to 0x100000 (1MB mark)...\r\n");
    uint8_t *kernelStart = (uint8_t *)0x100000;
    memcpy(kernelStart, kernelBuffer, fd->Size);
    printf(LOG_OK "Kernel copied to 0x100000\r\n");
    printf("\r\n");
    
    // Step 6: Transfer control to kernel
    printf(LOG_INFO "Step 6: Transferring control to kernel...\r\n");
    printf(LOG_INFO "Jumping to kernel entry point at 0x100000\r\n");
    printf("**************************************************\r\n");
    printf("*           KERNEL HANDOFF IN PROGRESS          *\r\n");
    printf("**************************************************\r\n");
    printf("\r\n");
    
    typedef void (*KernelStart)();
    KernelStart kernel = (KernelStart)kernelStart;
    kernel();

end:
    printf("\r\n");
    printf("**************************************************\r\n");
    printf("*              SYSTEM HALTED                    *\r\n");
    printf("**************************************************\r\n");
    for (;;)
        ;
}
