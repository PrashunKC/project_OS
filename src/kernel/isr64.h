#ifndef ISR64_H
#define ISR64_H

#include "stdint.h"

// Interrupt frame pushed by CPU and our stub
typedef struct {
  uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
  uint64_t rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax;
  uint64_t int_no, err_code;
  uint64_t rip, cs, rflags, userrsp, ss;
} __attribute__((packed)) InterruptFrame;

// Interrupt handler function pointer
typedef void (*ISRHandler)(InterruptFrame *frame);

// Functions
void isr64_init(void);
void register_interrupt_handler(uint8_t n, ISRHandler handler);

#endif // ISR64_H
