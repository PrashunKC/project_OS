#pragma once
#include "stdint.h"

// IDT Entry structure
typedef struct
{
    uint16_t base_low;  // Lower 16 bits of the address to jump to
    uint16_t sel;       // Kernel segment selector
    uint8_t  always0;   // This must always be 0
    uint8_t  flags;     // Flags (Present, Ring 0, etc)
    uint16_t base_high; // Upper 16 bits of the address to jump to
} __attribute__((packed)) IDTEntry;

// IDT Pointer structure
typedef struct
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) IDTPtr;

// Initialize the IDT
void idt_init();

// Set an entry in the IDT
void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags);
