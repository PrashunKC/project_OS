#include "idt.h"

// Declare the IDT and pointer
IDTEntry idt[256];
IDTPtr idt_ptr;

// External assembly function to load the IDT
extern void idt_load();

void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void idt_init()
{
    idt_ptr.limit = (sizeof(IDTEntry) * 256) - 1;
    idt_ptr.base = (uint32_t)&idt;

    // Initialize the IDT with zeros
    // memset(&idt, 0, sizeof(IDTEntry) * 256); // We don't have memset yet, but global variables are 0-initialized

    // We will populate the gates in the ISR initialization

    // Load the IDT
    idt_load();
}
