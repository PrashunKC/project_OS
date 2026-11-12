#pragma once
#include "stdint.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20

// Remap the master/slave PIC vectors to IRQ32-47.
void i8259_init();

// Acknowledge an interrupt on the master, and optionally the slave, PIC.
void i8259_send_eoi(int pic);

// Mask all IRQ lines on both PICs.
void i8259_disable();
