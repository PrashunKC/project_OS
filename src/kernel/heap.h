#pragma once
#include "stdint.h"

// Initialize the kernel heap
void heap_init(void);

// Allocate memory from the heap
void *kmalloc(uint64_t size);

// Allocate zeroed memory
void *kcalloc(uint64_t count, uint64_t size);

// Free previously allocated memory
void kfree(void *ptr);

// Reallocate memory block
void *krealloc(void *ptr, uint64_t new_size);

// Get heap statistics
typedef struct {
    uint64_t total_size;       // Total heap size
    uint64_t used_size;        // Currently allocated
    uint64_t free_size;        // Currently free
    uint64_t num_allocations;  // Number of active allocations
    uint64_t num_free_blocks;  // Number of free blocks
    uint64_t largest_free;     // Largest free block
} HeapStats;

void heap_get_stats(HeapStats *stats);

// Debug: print heap info
void heap_dump(void);
