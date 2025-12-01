#include "idt.h"

// Declare the IDT and pointer
IDTEntry idt[256];
IDTPtr idt_ptr;

// External assembly function to load the IDT
extern void idt_load();

void idt_set_gate(int num, uint64_t base, uint16_t sel, uint8_t flags)
{
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_mid = (base >> 16) & 0xFFFF;
    idt[num].base_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].sel = sel;
    idt[num].ist = 0;       // No IST for now
    idt[num].flags = flags;
    idt[num].reserved = 0;
}

void idt_init()
{
    idt_ptr.limit = (sizeof(IDTEntry) * 256) - 1;
    idt_ptr.base = (uint64_t)&idt;

    // Clear IDT
    for (int i = 0; i < 256; i++) {
        idt[i].base_low = 0;
        idt[i].base_mid = 0;
        idt[i].base_high = 0;
        idt[i].sel = 0;
        idt[i].ist = 0;
        idt[i].flags = 0;
        idt[i].reserved = 0;
    }

    // We will populate the gates in the ISR initialization

    // Load the IDT
    idt_load();
}
