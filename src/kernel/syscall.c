/*
 * NBOS System Call Implementation
 */

#include "syscall.h"
#include "graphics.h"
#include "keyboard.h"
#include "heap.h"
#include "linux_syscall.h"
#include "stdint.h"

// Linux ABI compatibility flag
// If set, syscall numbers are interpreted as Linux syscall numbers
static int linux_abi_mode = 0;

// External functions from main.c
void kprint(const char *str, uint8_t color);
void kputc(char c, uint8_t color);
void knewline(void);

// Syscall handler table
static SyscallHandler syscall_table[MAX_SYSCALL];

// Current program state
static int current_program_running = 0;
static int program_exit_code = 0;

/* ============================================================================
 * Syscall Implementations
 * ============================================================================ */

// SYS_EXIT (0) - Exit the current program
static uint64_t sys_exit(uint64_t code, uint64_t arg2, uint64_t arg3,
                         uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    program_exit_code = (int)code;
    current_program_running = 0;
    
    // For now, just print exit message and return to shell
    kprint("\n[Program exited with code ", 0x07);
    
    // Simple number print
    char buf[16];
    int i = 0;
    uint64_t n = code;
    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    buf[i] = '\0';
    // Reverse
    for (int j = 0; j < i / 2; j++) {
        char t = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = t;
    }
    kprint(buf, 0x07);
    kprint("]\n", 0x07);
    
    return 0;
}

// SYS_PRINT (1) - Print string to console
static uint64_t sys_print(uint64_t str_ptr, uint64_t arg2, uint64_t arg3,
                          uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    const char *str = (const char *)str_ptr;
    if (str == 0) return 0;
    
    // Print using VGA text mode
    while (*str) {
        if (*str == '\n') {
            knewline();
        } else {
            kputc(*str, 0x07); // Light gray on black
        }
        str++;
    }
    
    return 0;
}

// SYS_GETKEY (2) - Get keypress (blocking)
static uint64_t sys_getkey(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                           uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    // Wait for a key
    while (!keyboard_has_key()) {
        __asm__ volatile("hlt");
    }
    
    return (uint64_t)keyboard_get_key();
}

// SYS_KBHIT (3) - Check if key available
static uint64_t sys_kbhit(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                          uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    return keyboard_has_key() ? 1 : 0;
}

// SYS_MALLOC (4) - Allocate memory
static uint64_t sys_malloc(uint64_t size, uint64_t arg2, uint64_t arg3,
                           uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    void *ptr = kmalloc(size);
    return (uint64_t)ptr;
}

// SYS_FREE (5) - Free memory
static uint64_t sys_free(uint64_t ptr, uint64_t arg2, uint64_t arg3,
                         uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    kfree((void *)ptr);
    return 0;
}

// SYS_SLEEP (6) - Sleep for milliseconds
static uint64_t sys_sleep(uint64_t ms, uint64_t arg2, uint64_t arg3,
                          uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    // Simple busy-wait delay
    // TODO: Use PIT timer for accurate timing
    volatile uint64_t count = ms * 100000;
    while (count--) {
        __asm__ volatile("pause");
    }
    
    return 0;
}

// SYS_GETPID (7) - Get process ID
static uint64_t sys_getpid(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                           uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    return 1; // Always PID 1 for now (single process)
}

// SYS_PUTPIXEL (10) - Draw a pixel
static uint64_t sys_putpixel(uint64_t x, uint64_t y, uint64_t color,
                             uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    
    if (graphics_is_available()) {
        graphics_put_pixel((int)x, (int)y, (uint32_t)color);
    }
    return 0;
}

// SYS_GETPIXEL (11) - Get pixel color
static uint64_t sys_getpixel(uint64_t x, uint64_t y, uint64_t arg3,
                             uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    if (graphics_is_available()) {
        return graphics_get_pixel((int)x, (int)y);
    }
    return 0;
}

// SYS_CLEAR (12) - Clear screen
static uint64_t sys_clear(uint64_t color, uint64_t arg2, uint64_t arg3,
                          uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    if (graphics_is_available()) {
        graphics_clear((uint32_t)color);
    }
    return 0;
}

// SYS_GETWIDTH (13) - Get screen width
static uint64_t sys_getwidth(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                             uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    if (graphics_is_available()) {
        FramebufferInfo *fb = graphics_get_info();
        return fb->width;
    }
    return 80; // Text mode width
}

// SYS_GETHEIGHT (14) - Get screen height
static uint64_t sys_getheight(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                              uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    if (graphics_is_available()) {
        FramebufferInfo *fb = graphics_get_info();
        return fb->height;
    }
    return 25; // Text mode height
}

// SYS_DRAWTEXT (18) - Draw text at position
static uint64_t sys_drawtext(uint64_t x, uint64_t y, uint64_t str_ptr,
                             uint64_t colors, uint64_t arg5, uint64_t arg6) {
    (void)arg5; (void)arg6;
    
    const char *str = (const char *)str_ptr;
    if (!str) return 0;
    
    if (graphics_is_available()) {
        uint32_t fg = (uint32_t)(colors >> 32);
        uint32_t bg = (uint32_t)(colors & 0xFFFFFFFF);
        graphics_draw_string((int)x, (int)y, str, fg, bg);
    }
    
    return 0;
}

// SYS_GETFB (19) - Get framebuffer address
static uint64_t sys_getfb(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                          uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    if (graphics_is_available()) {
        FramebufferInfo *fb = graphics_get_info();
        return fb->framebuffer_addr;
    }
    return 0;
}

// SYS_MEMINFO (40) - Get memory info
// Returns: pointer to struct {total, used, free} stored at arg1
static uint64_t sys_meminfo(uint64_t info_ptr, uint64_t arg2, uint64_t arg3,
                            uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    HeapStats stats;
    heap_get_stats(&stats);
    
    if (info_ptr != 0) {
        uint64_t *info = (uint64_t *)info_ptr;
        info[0] = stats.total_size;
        info[1] = stats.used_size;
        info[2] = stats.free_size;
        info[3] = stats.num_allocations;
    }
    
    return stats.free_size;
}

// SYS_REALLOC (41) - Reallocate memory
static uint64_t sys_realloc(uint64_t ptr, uint64_t new_size, uint64_t arg3,
                            uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    void *new_ptr = krealloc((void *)ptr, new_size);
    return (uint64_t)new_ptr;
}

// SYS_CALLOC (42) - Allocate zeroed memory
static uint64_t sys_calloc(uint64_t count, uint64_t size, uint64_t arg3,
                           uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    void *ptr = kcalloc(count, size);
    return (uint64_t)ptr;
}

/* ============================================================================
 * Syscall Dispatcher
 * ============================================================================ */

void syscall_handler(Registers *regs) {
    uint64_t syscall_num = regs->rax;
    
    // Check if this is a Linux syscall
    // Linux syscalls use syscall instruction and have specific patterns
    // We detect Linux mode by:
    // 1. Syscall number being in Linux range (0-329) AND
    // 2. Linux ABI mode being enabled OR syscall is explicitly Linux
    
    if (linux_abi_mode) {
        // Route to Linux syscall handler
        regs->rax = linux_syscall_handler(syscall_num, 
                                          regs->rdi, regs->rsi, regs->rdx,
                                          regs->r10, regs->r8, regs->r9);
        return;
    }
    
    // NBOS native syscalls
    if (syscall_num >= MAX_SYSCALL || syscall_table[syscall_num] == 0) {
        // Invalid syscall
        regs->rax = (uint64_t)-1;
        return;
    }
    
    // Get arguments from registers
    // In our ISR, we saved registers in this order:
    // r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax
    // So we need to access them from the Registers struct
    
    SyscallHandler handler = syscall_table[syscall_num];
    regs->rax = handler(regs->rdi, regs->rsi, regs->rdx, 
                        regs->r10, regs->r8, regs->r9);
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

void syscall_register(uint64_t num, SyscallHandler handler) {
    if (num < MAX_SYSCALL) {
        syscall_table[num] = handler;
    }
}

void syscall_init(void) {
    // Clear syscall table
    for (int i = 0; i < MAX_SYSCALL; i++) {
        syscall_table[i] = 0;
    }
    
    // Register syscalls
    syscall_register(SYS_EXIT, sys_exit);
    syscall_register(SYS_PRINT, sys_print);
    syscall_register(SYS_GETKEY, sys_getkey);
    syscall_register(SYS_KBHIT, sys_kbhit);
    syscall_register(SYS_MALLOC, sys_malloc);
    syscall_register(SYS_FREE, sys_free);
    syscall_register(SYS_SLEEP, sys_sleep);
    syscall_register(SYS_GETPID, sys_getpid);
    
    // Graphics syscalls
    syscall_register(SYS_PUTPIXEL, sys_putpixel);
    syscall_register(SYS_GETPIXEL, sys_getpixel);
    syscall_register(SYS_CLEAR, sys_clear);
    syscall_register(SYS_GETWIDTH, sys_getwidth);
    syscall_register(SYS_GETHEIGHT, sys_getheight);
    syscall_register(SYS_DRAWTEXT, sys_drawtext);
    syscall_register(SYS_GETFB, sys_getfb);
    
    // Memory syscalls
    syscall_register(SYS_MEMINFO, sys_meminfo);
    syscall_register(SYS_REALLOC, sys_realloc);
    syscall_register(SYS_CALLOC, sys_calloc);
}

// Accessor functions
int syscall_is_program_running(void) {
    return current_program_running;
}

void syscall_set_program_running(int running) {
    current_program_running = running;
}

int syscall_get_exit_code(void) {
    return program_exit_code;
}

// Reset heap for new program (reinitialize the heap)
void syscall_reset_heap(void) {
    heap_init();
}

// Linux ABI mode control
void syscall_set_linux_mode(int enable) {
    linux_abi_mode = enable;
    if (enable) {
        linux_syscall_init();
    }
}

int syscall_get_linux_mode(void) {
    return linux_abi_mode;
}