#include "i8259.h"

// Helper functions to read/write ports (inline assembly)
static inline void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void io_wait(void) {
  // Port 0x80 is used for 'checkpoints' during POST.
  // The Linux kernel seems to think it is free for use :-/
  __asm__ volatile("outb %%al, $0x80" : : "a"(0));
}

void i8259_init() {
  // Remap PIC
  // Master PIC: 0x20 - 0x27
  // Slave PIC:  0x28 - 0x2F
  // We want to remap them to avoid conflict with CPU exceptions (0x00 - 0x1F)
  // Let's remap to 0x20 (32) and 0x28 (40)

  unsigned char a1, a2;

  a1 = inb(PIC1_DATA); // save masks
  a2 = inb(PIC2_DATA);

  outb(PIC1_COMMAND,
       0x11); // starts the initialization sequence (in cascade mode)
  io_wait();
  outb(PIC2_COMMAND, 0x11);
  io_wait();
  outb(PIC1_DATA, 0x20); // ICW2: Master PIC vector offset
  io_wait();
  outb(PIC2_DATA, 0x28); // ICW2: Slave PIC vector offset
  io_wait();
  outb(PIC1_DATA, 0x04); // ICW3: tell Master PIC that there is a slave PIC at
                         // IRQ2 (0000 0100)
  io_wait();
  outb(PIC2_DATA,
       0x02); // ICW3: tell Slave PIC its cascade identity (0000 0010)
  io_wait();

  outb(PIC1_DATA, 0x01); // ICW4: have the PICs use 8086 mode (and not 8085)
  io_wait();
  outb(PIC2_DATA, 0x01);
  io_wait();

  outb(PIC1_DATA, a1); // restore saved masks.
  outb(PIC2_DATA, a2);
}

void i8259_send_eoi(int pic) {
  if (pic) // Slave
  {
    outb(PIC2_COMMAND, PIC_EOI);
  }
  outb(PIC1_COMMAND, PIC_EOI);
}

void i8259_disable() {
  outb(PIC1_DATA, 0xFF);
  outb(PIC2_DATA, 0xFF);
}
