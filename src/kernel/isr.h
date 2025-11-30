#pragma once
#include "stdint.h"

// Registers structure to capture CPU state
typedef struct {
  uint32_t ds;                                     // Data segment selector
  uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
  uint32_t int_no, err_code; // Interrupt number and error code (if applicable)
  uint32_t eip, cs, eflags, useresp,
      ss; // Pushed by the processor automatically
} __attribute__((packed)) Registers;

// ISR handler function prototype
void isr_handler(Registers *regs);
void irq_handler(Registers *regs);

// Function to register interrupt handlers (for future use)
typedef void (*ISRHandler)(Registers *regs);
void register_interrupt_handler(uint8_t n, ISRHandler handler);
void isr_init();
