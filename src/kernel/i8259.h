#pragma once
#include "stdint.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20

void i8259_init();
void i8259_send_eoi(int pic);
void i8259_disable();
