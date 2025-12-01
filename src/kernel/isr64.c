#include "isr64.h"
#include "idt64.h"

// External ISR stubs from assembly
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

// External IRQ stubs from assembly
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

// External declarations
extern void kprint(const char *str, uint8_t color);
extern void kputc(char c, uint8_t color);
extern void knewline(void);

// Handler array
static ISRHandler interrupt_handlers[256];

void isr64_init(void) {
  uint8_t flags = IDT64_FLAG_PRESENT | IDT64_FLAG_RING0 | IDT64_FLAG_GATE_INT;
  uint16_t code_segment = 0x08; // Code segment from GDT in long_mode_init.asm

  // Install ISRs (CPU exceptions)
  idt64_set_gate(0, (uint64_t)isr0, code_segment, flags);
  idt64_set_gate(1, (uint64_t)isr1, code_segment, flags);
  idt64_set_gate(2, (uint64_t)isr2, code_segment, flags);
  idt64_set_gate(3, (uint64_t)isr3, code_segment, flags);
  idt64_set_gate(4, (uint64_t)isr4, code_segment, flags);
  idt64_set_gate(5, (uint64_t)isr5, code_segment, flags);
  idt64_set_gate(6, (uint64_t)isr6, code_segment, flags);
  idt64_set_gate(7, (uint64_t)isr7, code_segment, flags);
  idt64_set_gate(8, (uint64_t)isr8, code_segment, flags);
  idt64_set_gate(9, (uint64_t)isr9, code_segment, flags);
  idt64_set_gate(10, (uint64_t)isr10, code_segment, flags);
  idt64_set_gate(11, (uint64_t)isr11, code_segment, flags);
  idt64_set_gate(12, (uint64_t)isr12, code_segment, flags);
  idt64_set_gate(13, (uint64_t)isr13, code_segment, flags);
  idt64_set_gate(14, (uint64_t)isr14, code_segment, flags);
  idt64_set_gate(15, (uint64_t)isr15, code_segment, flags);
  idt64_set_gate(16, (uint64_t)isr16, code_segment, flags);
  idt64_set_gate(17, (uint64_t)isr17, code_segment, flags);
  idt64_set_gate(18, (uint64_t)isr18, code_segment, flags);
  idt64_set_gate(19, (uint64_t)isr19, code_segment, flags);
  idt64_set_gate(20, (uint64_t)isr20, code_segment, flags);
  idt64_set_gate(21, (uint64_t)isr21, code_segment, flags);
  idt64_set_gate(22, (uint64_t)isr22, code_segment, flags);
  idt64_set_gate(23, (uint64_t)isr23, code_segment, flags);
  idt64_set_gate(24, (uint64_t)isr24, code_segment, flags);
  idt64_set_gate(25, (uint64_t)isr25, code_segment, flags);
  idt64_set_gate(26, (uint64_t)isr26, code_segment, flags);
  idt64_set_gate(27, (uint64_t)isr27, code_segment, flags);
  idt64_set_gate(28, (uint64_t)isr28, code_segment, flags);
  idt64_set_gate(29, (uint64_t)isr29, code_segment, flags);
  idt64_set_gate(30, (uint64_t)isr30, code_segment, flags);
  idt64_set_gate(31, (uint64_t)isr31, code_segment, flags);

  // Install IRQs (hardware interrupts)
  idt64_set_gate(32, (uint64_t)irq0, code_segment, flags);
  idt64_set_gate(33, (uint64_t)irq1, code_segment, flags);
  idt64_set_gate(34, (uint64_t)irq2, code_segment, flags);
  idt64_set_gate(35, (uint64_t)irq3, code_segment, flags);
  idt64_set_gate(36, (uint64_t)irq4, code_segment, flags);
  idt64_set_gate(37, (uint64_t)irq5, code_segment, flags);
  idt64_set_gate(38, (uint64_t)irq6, code_segment, flags);
  idt64_set_gate(39, (uint64_t)irq7, code_segment, flags);
  idt64_set_gate(40, (uint64_t)irq8, code_segment, flags);
  idt64_set_gate(41, (uint64_t)irq9, code_segment, flags);
  idt64_set_gate(42, (uint64_t)irq10, code_segment, flags);
  idt64_set_gate(43, (uint64_t)irq11, code_segment, flags);
  idt64_set_gate(44, (uint64_t)irq12, code_segment, flags);
  idt64_set_gate(45, (uint64_t)irq13, code_segment, flags);
  idt64_set_gate(46, (uint64_t)irq14, code_segment, flags);
  idt64_set_gate(47, (uint64_t)irq15, code_segment, flags);

  // Clear handler array
  for (int i = 0; i < 256; i++) {
    interrupt_handlers[i] = 0;
  }
  // Register Page Fault handler
  void page_fault_handler(InterruptFrame * frame);
  register_interrupt_handler(14, page_fault_handler);
}

void register_interrupt_handler(uint8_t n, ISRHandler handler) {
  interrupt_handlers[n] = handler;
}

void isr_handler(InterruptFrame *frame) {
  if (interrupt_handlers[frame->int_no] != 0) {
    ISRHandler handler = interrupt_handlers[frame->int_no];
    handler(frame);
  } else {
    // Unhandled exception
    kprint("Unhandled Exception: ", 0x0C);
    // Print interrupt number (hacky hex print)
    char hex[] = "0123456789ABCDEF";
    char buf[3];
    buf[0] = hex[(frame->int_no >> 4) & 0xF];
    buf[1] = hex[frame->int_no & 0xF];
    buf[2] = 0;
    kprint(buf, 0x0C);

    kprint(" Halted.", 0x0C);
    for (;;)
      __asm__ volatile("hlt");
  }
}

void irq_handler(InterruptFrame *frame) {
  // Send EOI to PIC
  extern void i8259_send_eoi(int pic);
  if (frame->int_no >= 40) {
    i8259_send_eoi(1); // Slave PIC (sends to both)
  } else {
    i8259_send_eoi(0); // Master PIC
  }

  // Call registered handler
  if (interrupt_handlers[frame->int_no] != 0) {
    ISRHandler handler = interrupt_handlers[frame->int_no];
    handler(frame);
  }
}

// Helper to read CR2
static inline uint64_t get_cr2(void) {
  uint64_t cr2;
  __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
  return cr2;
}

// Page Fault Handler
void page_fault_handler(InterruptFrame *frame) {
  uint64_t cr2 = get_cr2();
  kprint("Page Fault at 0x", 0x0C);

  // Print CR2 (hex)
  char hex[] = "0123456789ABCDEF";
  for (int i = 60; i >= 0; i -= 4) {
    char c = hex[(cr2 >> i) & 0xF];
    kputc(c, 0x0C);
  }

  kprint(" IP:0x", 0x0C);
  // Print RIP
  for (int i = 60; i >= 0; i -= 4) {
    char c = hex[(frame->rip >> i) & 0xF];
    kputc(c, 0x0C);
  }

  kprint(" Halted.", 0x0C);
  for (;;)
    __asm__ volatile("hlt");
}
