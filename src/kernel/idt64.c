#include "idt64.h"

// IDT with 256 entries
static IDT64Entry idt[256] __attribute__((aligned(16)));
static IDT64Ptr idt_ptr;

// External assembly function to load IDT
extern void idt64_load(uint64_t idt_ptr_addr);

void idt64_set_gate(int n, uint64_t base, uint16_t selector, uint8_t flags) {
  idt[n].base_low = base & 0xFFFF;
  idt[n].base_mid = (base >> 16) & 0xFFFF;
  idt[n].base_high = (base >> 32) & 0xFFFFFFFF;

  idt[n].selector = selector;
  idt[n].ist = 0; // Not using IST for now
  idt[n].flags = flags;
  idt[n].reserved = 0;
}

void idt64_init(void) {
  idt_ptr.limit = (sizeof(IDT64Entry) * 256) - 1;
  idt_ptr.base = (uint64_t)&idt;

  // Clear all entries
  for (int i = 0; i < 256; i++) {
    idt64_set_gate(i, 0, 0, 0);
  }

  // Load the IDT
  idt64_load((uint64_t)&idt_ptr);
}
