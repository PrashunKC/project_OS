#include "stdint.h"

#define _cdecl __attribute__((cdecl))

void _cdecl start(uint16_t bootDrive)
{
    // Write "Hello world from kernel" to video memory
    const char *str = "Hello world from kernel";
    uint8_t *video = (uint8_t *)0xB8000;
    
    // Clear screen
    for(int i = 0; i < 80 * 25 * 2; i += 2) {
        video[i] = ' ';
        video[i+1] = 0x07;
    }
    
    // Write string
    int i = 0;
    while (str[i])
    {
        video[i * 2] = str[i];
        video[i * 2 + 1] = 0x0A; // Light Green
        i++;
    }
    
    for (;;);
}
