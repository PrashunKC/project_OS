#ifndef IDT64_H
#define IDT64_H

#include "stdint.h"

// 64-bit IDT Gate Descriptor (16 bytes)
// Following Intel notation to match existing 32-bit code
typedef struct {
  uint16_t base_low;  // Offset bits 0-15
  uint16_t selector;  // Code segment selector
  uint8_t ist;        // Interrupt Stack Table offset (bits 0-2), rest reserved
  uint8_t flags;      // Type and attributes (gate type, DPL, P)
  uint16_t base_mid;  // Offset bits 16-31
  uint32_t base_high; // Offset bits 32-63
  uint32_t reserved;  // Reserved, must be 0
} __attribute__((packed)) IDT64Entry;

// IDT Pointer structure
typedef struct {
  uint16_t limit; // Size of IDT - 1
  uint64_t base;  // Base address of IDT
} __attribute__((packed)) IDT64Ptr;

// Flag definitions
#define IDT64_FLAG_PRESENT 0x80
#define IDT64_FLAG_RING0 0x00
#define IDT64_FLAG_RING3 0x60
#define IDT64_FLAG_GATE_INT 0x0E  // 64-bit Interrupt Gate
#define IDT64_FLAG_GATE_TRAP 0x0F // 64-bit Trap Gate

// Function prototypes
void idt64_init(void);
void idt64_set_gate(int n, uint64_t base, uint16_t selector, uint8_t flags);

#endif // IDT64_H
