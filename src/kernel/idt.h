#pragma once
#include "stdint.h"

// 64-bit interrupt gate descriptor used by the active IDT.
typedef struct
{
    uint16_t base_low;     // Lower 16 bits of handler address
    uint16_t sel;          // Kernel code segment selector
    uint8_t  ist;          // Interrupt Stack Table offset (bits 0-2), rest must be 0
    uint8_t  flags;        // Type and attributes
    uint16_t base_mid;     // Middle 16 bits of handler address
    uint32_t base_high;    // Upper 32 bits of handler address
    uint32_t reserved;     // Reserved, must be 0
} __attribute__((packed)) IDTEntry;

// Operand consumed by the lidt instruction.
typedef struct
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) IDTPtr;

// Clear the 256-entry IDT and load it with the assembly helper.
void idt_init();

// Install one interrupt gate at num.
void idt_set_gate(int num, uint64_t base, uint16_t sel, uint8_t flags);
