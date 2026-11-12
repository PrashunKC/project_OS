#pragma once
#include "stdint.h"

// Register frame created by the assembly exception/IRQ stubs.
typedef struct {
  // General purpose registers (pushed by our stub)
  uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
  uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
  
  // Interrupt number and error code
  uint64_t int_no, err_code;
  
  // Pushed by the processor automatically
  uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) Registers;

// Dispatch an exception or hardware IRQ using the saved register frame.
void isr_handler(Registers *regs);
void irq_handler(Registers *regs);

// Callback type and registration API for vectors 0-255.
typedef void (*ISRHandler)(Registers *regs);
void register_interrupt_handler(uint8_t n, ISRHandler handler);

// Install exception and IRQ gates in the IDT.
void isr_init();
