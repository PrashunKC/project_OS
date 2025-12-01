#pragma once
#include "stdint.h"

// 64-bit Registers structure to capture CPU state
typedef struct {
  // General purpose registers (pushed by our stub)
  uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
  uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
  
  // Interrupt number and error code
  uint64_t int_no, err_code;
  
  // Pushed by the processor automatically
  uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) Registers;

// ISR handler function prototype
void isr_handler(Registers *regs);
void irq_handler(Registers *regs);

// Function to register interrupt handlers
typedef void (*ISRHandler)(Registers *regs);
void register_interrupt_handler(uint8_t n, ISRHandler handler);
void isr_init();
