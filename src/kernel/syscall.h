/*
 * NBOS System Call Implementation
 * 
 * Provides the syscall interface for user programs.
 * Uses INT 0x80 for syscall invocation (Linux-style).
 */

#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "stdint.h"
#include "isr.h"

/* ============================================================================
 * Syscall Numbers
 * ============================================================================ */

#define SYS_EXIT        0   // Exit program
#define SYS_PRINT       1   // Print string to console
#define SYS_GETKEY      2   // Get keypress (blocking)
#define SYS_KBHIT       3   // Check if key available
#define SYS_MALLOC      4   // Allocate memory
#define SYS_FREE        5   // Free memory
#define SYS_SLEEP       6   // Sleep for milliseconds
#define SYS_GETPID      7   // Get process ID (always 1 for now)
#define SYS_READ        8   // Read from file/stdin
#define SYS_WRITE       9   // Write to file/stdout

// Graphics syscalls (10-19)
#define SYS_PUTPIXEL    10  // Draw pixel
#define SYS_GETPIXEL    11  // Get pixel color
#define SYS_CLEAR       12  // Clear screen
#define SYS_GETWIDTH    13  // Get screen width
#define SYS_GETHEIGHT   14  // Get screen height
#define SYS_DRAWLINE    15  // Draw line
#define SYS_DRAWRECT    16  // Draw rectangle
#define SYS_FILLRECT    17  // Fill rectangle
#define SYS_DRAWTEXT    18  // Draw text at position
#define SYS_GETFB       19  // Get framebuffer address

// File syscalls (20-29) - future
#define SYS_OPEN        20  // Open file
#define SYS_CLOSE       21  // Close file
#define SYS_SEEK        22  // Seek in file
#define SYS_STAT        23  // Get file info

// Process syscalls (30-39) - future
#define SYS_EXEC        30  // Execute program
#define SYS_FORK        31  // Fork process (future)
#define SYS_WAIT        32  // Wait for process

// Memory info syscalls (40-49)
#define SYS_MEMINFO     40  // Get memory info (total, used, free)
#define SYS_REALLOC     41  // Reallocate memory
#define SYS_CALLOC      42  // Allocate zeroed memory

#define MAX_SYSCALL     64

/* ============================================================================
 * Syscall Registers (passed via interrupt frame)
 * 
 * System V AMD64 ABI for syscalls:
 *   RAX = syscall number
 *   RDI = arg1
 *   RSI = arg2
 *   RDX = arg3
 *   R10 = arg4 (RCX is clobbered by syscall instruction)
 *   R8  = arg5
 *   R9  = arg6
 *   RAX = return value
 * ============================================================================ */

typedef uint64_t (*SyscallHandler)(uint64_t arg1, uint64_t arg2, 
                                    uint64_t arg3, uint64_t arg4,
                                    uint64_t arg5, uint64_t arg6);

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

// Initialize syscall subsystem
void syscall_init(void);

// Register a syscall handler
void syscall_register(uint64_t num, SyscallHandler handler);

// Syscall interrupt handler (called from assembly)
void syscall_handler(Registers *regs);

// Program state management
int syscall_is_program_running(void);
void syscall_set_program_running(int running);
int syscall_get_exit_code(void);
void syscall_reset_heap(void);

// Linux ABI compatibility mode
void syscall_set_linux_mode(int enable);
int syscall_get_linux_mode(void);

#endif /* _SYSCALL_H */
