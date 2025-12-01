#include "heap.h"
#include "console.h"

// Heap configuration
#define HEAP_START      0x400000    // 4MB - start of heap
#define HEAP_SIZE       0x1000000   // 16MB heap size
#define HEAP_END        (HEAP_START + HEAP_SIZE)

#define BLOCK_MAGIC     0xDEADBEEF
#define BLOCK_FREE      0x00
#define BLOCK_USED      0x01

// Block header structure
typedef struct BlockHeader {
    uint32_t magic;              // Magic number for validation
    uint32_t flags;              // BLOCK_FREE or BLOCK_USED
    uint64_t size;               // Size of data area (not including header)
    struct BlockHeader *next;    // Next block in list
    struct BlockHeader *prev;    // Previous block in list
} __attribute__((packed)) BlockHeader;

#define HEADER_SIZE     sizeof(BlockHeader)
#define MIN_BLOCK_SIZE  32  // Minimum allocation size

// Global heap state
static BlockHeader *heap_start = NULL;
static BlockHeader *free_list = NULL;
static uint64_t total_allocated = 0;
static uint64_t num_allocations = 0;

// Initialize the heap
void heap_init(void) {
    heap_start = (BlockHeader *)HEAP_START;
    
    // Create one large free block
    heap_start->magic = BLOCK_MAGIC;
    heap_start->flags = BLOCK_FREE;
    heap_start->size = HEAP_SIZE - HEADER_SIZE;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    
    free_list = heap_start;
    total_allocated = 0;
    num_allocations = 0;
}

// Find a free block that fits the requested size
static BlockHeader *find_free_block(uint64_t size) {
    BlockHeader *current = free_list;
    BlockHeader *best_fit = NULL;
    
    // Best-fit search
    while (current != NULL) {
        if (current->flags == BLOCK_FREE && current->size >= size) {
            if (best_fit == NULL || current->size < best_fit->size) {
                best_fit = current;
                // If exact match, use it immediately
                if (current->size == size) {
                    break;
                }
            }
        }
        current = current->next;
    }
    
    return best_fit;
}

// Split a block if it's large enough
static void split_block(BlockHeader *block, uint64_t size) {
    uint64_t remaining = block->size - size - HEADER_SIZE;
    
    // Only split if remaining is large enough for a new block
    if (remaining >= MIN_BLOCK_SIZE) {
        BlockHeader *new_block = (BlockHeader *)((uint8_t *)block + HEADER_SIZE + size);
        
        new_block->magic = BLOCK_MAGIC;
        new_block->flags = BLOCK_FREE;
        new_block->size = remaining;
        new_block->next = block->next;
        new_block->prev = block;
        
        if (block->next != NULL) {
            block->next->prev = new_block;
        }
        block->next = new_block;
        block->size = size;
    }
}

// Remove a block from the free list
static void remove_from_free_list(BlockHeader *block) {
    if (block == free_list) {
        free_list = block->next;
    }
    // Note: blocks stay linked for coalescing, just marked as used
}

// Coalesce adjacent free blocks
static void coalesce(BlockHeader *block) {
    // Coalesce with next block if free
    if (block->next != NULL && block->next->flags == BLOCK_FREE) {
        block->size += HEADER_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next != NULL) {
            block->next->prev = block;
        }
    }
    
    // Coalesce with previous block if free
    if (block->prev != NULL && block->prev->flags == BLOCK_FREE) {
        block->prev->size += HEADER_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next != NULL) {
            block->next->prev = block->prev;
        }
    }
}

// Allocate memory
void *kmalloc(uint64_t size) {
    if (size == 0) {
        return NULL;
    }
    
    // Align size to 8 bytes
    size = (size + 7) & ~7ULL;
    
    // Ensure minimum size
    if (size < MIN_BLOCK_SIZE) {
        size = MIN_BLOCK_SIZE;
    }
    
    // Find a suitable free block
    BlockHeader *block = find_free_block(size);
    
    if (block == NULL) {
        // Out of memory
        return NULL;
    }
    
    // Split the block if it's too large
    split_block(block, size);
    
    // Mark as used
    block->flags = BLOCK_USED;
    remove_from_free_list(block);
    
    // Update statistics
    total_allocated += block->size;
    num_allocations++;
    
    // Return pointer to data area (after header)
    return (void *)((uint8_t *)block + HEADER_SIZE);
}

// Allocate and zero memory
void *kcalloc(uint64_t count, uint64_t size) {
    uint64_t total = count * size;
    void *ptr = kmalloc(total);
    
    if (ptr != NULL) {
        // Zero the memory
        uint8_t *p = (uint8_t *)ptr;
        for (uint64_t i = 0; i < total; i++) {
            p[i] = 0;
        }
    }
    
    return ptr;
}

// Free memory
void kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    
    // Get the block header
    BlockHeader *block = (BlockHeader *)((uint8_t *)ptr - HEADER_SIZE);
    
    // Validate the block
    if (block->magic != BLOCK_MAGIC) {
        // Invalid pointer - corrupted heap or bad free
        return;
    }
    
    if (block->flags != BLOCK_USED) {
        // Double free
        return;
    }
    
    // Update statistics
    total_allocated -= block->size;
    num_allocations--;
    
    // Mark as free
    block->flags = BLOCK_FREE;
    
    // Add back to free list (at the beginning for simplicity)
    if (free_list == NULL) {
        free_list = block;
    }
    
    // Coalesce with adjacent free blocks
    coalesce(block);
}

// Reallocate memory
void *krealloc(void *ptr, uint64_t new_size) {
    if (ptr == NULL) {
        return kmalloc(new_size);
    }
    
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    // Get current block
    BlockHeader *block = (BlockHeader *)((uint8_t *)ptr - HEADER_SIZE);
    
    if (block->magic != BLOCK_MAGIC || block->flags != BLOCK_USED) {
        return NULL;
    }
    
    // If new size fits in current block, just return
    if (new_size <= block->size) {
        return ptr;
    }
    
    // Allocate new block
    void *new_ptr = kmalloc(new_size);
    if (new_ptr == NULL) {
        return NULL;
    }
    
    // Copy data
    uint8_t *src = (uint8_t *)ptr;
    uint8_t *dst = (uint8_t *)new_ptr;
    for (uint64_t i = 0; i < block->size; i++) {
        dst[i] = src[i];
    }
    
    // Free old block
    kfree(ptr);
    
    return new_ptr;
}

// Get heap statistics
void heap_get_stats(HeapStats *stats) {
    if (stats == NULL) {
        return;
    }
    
    stats->total_size = HEAP_SIZE;
    stats->used_size = total_allocated;
    stats->free_size = HEAP_SIZE - total_allocated - (num_allocations * HEADER_SIZE);
    stats->num_allocations = num_allocations;
    stats->num_free_blocks = 0;
    stats->largest_free = 0;
    
    // Count free blocks and find largest
    BlockHeader *current = heap_start;
    while (current != NULL && (uint64_t)current < HEAP_END) {
        if (current->magic != BLOCK_MAGIC) {
            break;  // Corrupted heap
        }
        
        if (current->flags == BLOCK_FREE) {
            stats->num_free_blocks++;
            if (current->size > stats->largest_free) {
                stats->largest_free = current->size;
            }
        }
        
        current = current->next;
    }
}

// Debug: dump heap information
void heap_dump(void) {
    HeapStats stats;
    heap_get_stats(&stats);
    
    console_print("Heap Statistics:\n", CONSOLE_COLOR_YELLOW);
    
    // Print total size
    console_print("  Total: ", CONSOLE_COLOR_GRAY);
    
    char buf[32];
    int pos = 0;
    uint64_t n = stats.total_size / 1024;  // KB
    
    if (n == 0) {
        buf[pos++] = '0';
    } else {
        int digits[20];
        int count = 0;
        while (n > 0) {
            digits[count++] = n % 10;
            n /= 10;
        }
        for (int i = count - 1; i >= 0; i--) {
            buf[pos++] = '0' + digits[i];
        }
    }
    buf[pos++] = ' ';
    buf[pos++] = 'K';
    buf[pos++] = 'B';
    buf[pos++] = '\n';
    buf[pos] = '\0';
    console_print(buf, CONSOLE_COLOR_WHITE);
    
    // Print used
    console_print("  Used:  ", CONSOLE_COLOR_GRAY);
    pos = 0;
    n = stats.used_size / 1024;
    if (n == 0) {
        buf[pos++] = '0';
    } else {
        int digits[20];
        int count = 0;
        while (n > 0) {
            digits[count++] = n % 10;
            n /= 10;
        }
        for (int i = count - 1; i >= 0; i--) {
            buf[pos++] = '0' + digits[i];
        }
    }
    buf[pos++] = ' ';
    buf[pos++] = 'K';
    buf[pos++] = 'B';
    buf[pos++] = '\n';
    buf[pos] = '\0';
    console_print(buf, CONSOLE_COLOR_WHITE);
    
    // Print free
    console_print("  Free:  ", CONSOLE_COLOR_GRAY);
    pos = 0;
    n = stats.free_size / 1024;
    if (n == 0) {
        buf[pos++] = '0';
    } else {
        int digits[20];
        int count = 0;
        while (n > 0) {
            digits[count++] = n % 10;
            n /= 10;
        }
        for (int i = count - 1; i >= 0; i--) {
            buf[pos++] = '0' + digits[i];
        }
    }
    buf[pos++] = ' ';
    buf[pos++] = 'K';
    buf[pos++] = 'B';
    buf[pos++] = '\n';
    buf[pos] = '\0';
    console_print(buf, CONSOLE_COLOR_LIGHT_GREEN);
    
    // Print allocations count
    console_print("  Allocations: ", CONSOLE_COLOR_GRAY);
    pos = 0;
    n = stats.num_allocations;
    if (n == 0) {
        buf[pos++] = '0';
    } else {
        int digits[20];
        int count = 0;
        while (n > 0) {
            digits[count++] = n % 10;
            n /= 10;
        }
        for (int i = count - 1; i >= 0; i--) {
            buf[pos++] = '0' + digits[i];
        }
    }
    buf[pos++] = '\n';
    buf[pos] = '\0';
    console_print(buf, CONSOLE_COLOR_WHITE);
}
