#include "paging.h"
#include "stdint.h"

// Paging is now handled in entry.asm for 64-bit long mode
// These functions are stubs for compatibility

void paging_init(void) {
  // Paging already set up in entry.asm with identity mapping
  // First 1GB is identity mapped using 2MB pages
}

uint64_t get_cr0(void) {
  uint64_t cr0;
  __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
  return cr0;
}

uint64_t get_cr3(void) {
  uint64_t cr3;
  __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
  return cr3;
}

uint64_t get_cr4(void) {
  uint64_t cr4;
  __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
  return cr4;
}
