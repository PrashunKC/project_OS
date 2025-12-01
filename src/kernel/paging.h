#pragma once
#include "stdint.h"

// 64-bit Paging Constants
#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_USER 0x4
#define PAGE_WRITE_THROUGH 0x8
#define PAGE_CACHE_DISABLE 0x10
#define PAGE_ACCESSED 0x20
#define PAGE_DIRTY 0x40
#define PAGE_HUGE 0x80  // 2MB page (PSE)
#define PAGE_GLOBAL 0x100

// Page table entry types (all 64-bit in long mode)
typedef uint64_t pml4_entry_t;
typedef uint64_t pdpt_entry_t;
typedef uint64_t pd_entry_t;
typedef uint64_t pt_entry_t;

// Functions
void paging_init(void);
uint64_t get_cr0(void);
uint64_t get_cr3(void);
uint64_t get_cr4(void);
