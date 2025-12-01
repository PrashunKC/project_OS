/*
 * Linux Syscall Compatibility Layer Implementation
 * 
 * Provides Linux x86_64 syscall implementations for NBOS
 */

#include "linux_syscall.h"
#include "syscall.h"
#include "heap.h"
#include "graphics.h"
#include "keyboard.h"
#include "console.h"

// Forward declarations
extern void console_print(const char *str, uint32_t color);
extern void console_putchar(char c, uint32_t color);
extern void console_newline(void);

/* ============================================================================
 * Internal state
 * ============================================================================ */

// Program break (for brk syscall)
static uint64_t program_break = 0x800000;  // Start at 8MB
#define PROGRAM_BREAK_MAX 0x1000000        // Max 16MB

// Simple file descriptor table
#define MAX_FDS 16
static struct {
    int in_use;
    int type;       // 0=closed, 1=console, 2=file
    uint64_t pos;   // file position
} fd_table[MAX_FDS];

#define FD_TYPE_CLOSED  0
#define FD_TYPE_CONSOLE 1
#define FD_TYPE_FILE    2

// System uptime (ticks since boot, ~18.2 Hz from PIT)
static volatile uint64_t system_ticks = 0;

/* ============================================================================
 * Helper functions
 * ============================================================================ */

static void init_fd_table(void) {
    // stdin, stdout, stderr all point to console
    fd_table[0].in_use = 1;
    fd_table[0].type = FD_TYPE_CONSOLE;
    fd_table[0].pos = 0;
    
    fd_table[1].in_use = 1;
    fd_table[1].type = FD_TYPE_CONSOLE;
    fd_table[1].pos = 0;
    
    fd_table[2].in_use = 1;
    fd_table[2].type = FD_TYPE_CONSOLE;
    fd_table[2].pos = 0;
    
    for (int i = 3; i < MAX_FDS; i++) {
        fd_table[i].in_use = 0;
        fd_table[i].type = FD_TYPE_CLOSED;
        fd_table[i].pos = 0;
    }
}

static int alloc_fd(void) {
    for (int i = 3; i < MAX_FDS; i++) {
        if (!fd_table[i].in_use) {
            fd_table[i].in_use = 1;
            return i;
        }
    }
    return -1;  // No free FDs
}

static int is_valid_fd(int fd) {
    return fd >= 0 && fd < MAX_FDS && fd_table[fd].in_use;
}

/* ============================================================================
 * Linux syscall implementations
 * ============================================================================ */

// read(fd, buf, count) - syscall 0
static uint64_t linux_sys_read(uint64_t fd, uint64_t buf, uint64_t count,
                               uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    
    if (!is_valid_fd((int)fd)) {
        return (uint64_t)-9;  // -EBADF
    }
    
    char *buffer = (char *)buf;
    
    if (fd == STDIN_FILENO) {
        // Read from keyboard
        uint64_t bytes_read = 0;
        while (bytes_read < count) {
            // Wait for key
            while (!keyboard_has_key()) {
                __asm__ volatile("hlt");
            }
            char c = keyboard_get_key();
            
            // Handle special keys
            if (c == '\b') {
                if (bytes_read > 0) {
                    bytes_read--;
                    console_putchar('\b', 0xFFFFFF);
                }
                continue;
            }
            
            buffer[bytes_read++] = c;
            console_putchar(c, 0xFFFFFF);
            
            // Return on newline
            if (c == '\n') {
                break;
            }
        }
        return bytes_read;
    }
    
    // TODO: Implement file reading
    return (uint64_t)-9;  // -EBADF for now
}

// write(fd, buf, count) - syscall 1
static uint64_t linux_sys_write(uint64_t fd, uint64_t buf, uint64_t count,
                                uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg4; (void)arg5; (void)arg6;
    
    if (!is_valid_fd((int)fd)) {
        return (uint64_t)-9;  // -EBADF
    }
    
    const char *buffer = (const char *)buf;
    
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        // Write to console
        for (uint64_t i = 0; i < count; i++) {
            char c = buffer[i];
            if (c == '\n') {
                console_newline();
            } else {
                console_putchar(c, fd == STDERR_FILENO ? 0xFF6666 : 0xFFFFFF);
            }
        }
        return count;
    }
    
    // TODO: Implement file writing
    return (uint64_t)-9;  // -EBADF for now
}

// open(pathname, flags, mode) - syscall 2
static uint64_t linux_sys_open(uint64_t pathname, uint64_t flags, uint64_t mode,
                               uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)pathname; (void)flags; (void)mode;
    (void)arg4; (void)arg5; (void)arg6;
    
    // TODO: Implement file system
    return (uint64_t)-2;  // -ENOENT (file not found)
}

// close(fd) - syscall 3
static uint64_t linux_sys_close(uint64_t fd, uint64_t arg2, uint64_t arg3,
                                uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    if (!is_valid_fd((int)fd)) {
        return (uint64_t)-9;  // -EBADF
    }
    
    // Don't close stdin/stdout/stderr
    if (fd <= STDERR_FILENO) {
        return 0;
    }
    
    fd_table[fd].in_use = 0;
    fd_table[fd].type = FD_TYPE_CLOSED;
    return 0;
}

// fstat(fd, statbuf) - syscall 5
static uint64_t linux_sys_fstat(uint64_t fd, uint64_t statbuf, uint64_t arg3,
                                uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    if (!is_valid_fd((int)fd)) {
        return (uint64_t)-9;  // -EBADF
    }
    
    linux_stat_t *st = (linux_stat_t *)statbuf;
    
    // Zero out the structure
    for (int i = 0; i < (int)sizeof(linux_stat_t); i++) {
        ((uint8_t *)st)[i] = 0;
    }
    
    if (fd <= STDERR_FILENO) {
        // Character device (console)
        st->st_mode = 0020666;  // S_IFCHR | 0666
        st->st_blksize = 1024;
    }
    
    return 0;
}

// mmap(addr, length, prot, flags, fd, offset) - syscall 9
static uint64_t linux_sys_mmap(uint64_t addr, uint64_t length, uint64_t prot,
                               uint64_t flags, uint64_t fd, uint64_t offset) {
    (void)addr; (void)prot; (void)offset;
    
    // Only support anonymous mappings for now
    if (!(flags & MAP_ANONYMOUS)) {
        if ((int64_t)fd >= 0) {
            return (uint64_t)MAP_FAILED;  // -ENOSYS
        }
    }
    
    // Allocate memory from heap
    void *ptr = kmalloc(length);
    if (ptr == NULL) {
        return (uint64_t)MAP_FAILED;
    }
    
    // Zero the memory (MAP_ANONYMOUS should be zeroed)
    uint8_t *p = (uint8_t *)ptr;
    for (uint64_t i = 0; i < length; i++) {
        p[i] = 0;
    }
    
    return (uint64_t)ptr;
}

// munmap(addr, length) - syscall 11
static uint64_t linux_sys_munmap(uint64_t addr, uint64_t length, uint64_t arg3,
                                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)length; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    kfree((void *)addr);
    return 0;
}

// brk(addr) - syscall 12
static uint64_t linux_sys_brk(uint64_t addr, uint64_t arg2, uint64_t arg3,
                              uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    if (addr == 0) {
        // Query current break
        return program_break;
    }
    
    if (addr >= program_break && addr < PROGRAM_BREAK_MAX) {
        program_break = addr;
        return program_break;
    }
    
    // If trying to shrink or invalid, return current
    return program_break;
}

// ioctl(fd, request, ...) - syscall 16
static uint64_t linux_sys_ioctl(uint64_t fd, uint64_t request, uint64_t arg,
                                uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg; (void)arg4; (void)arg5; (void)arg6;
    
    if (!is_valid_fd((int)fd)) {
        return (uint64_t)-9;  // -EBADF
    }
    
    // TIOCGWINSZ - get window size
    if (request == 0x5413) {
        struct winsize {
            uint16_t ws_row;
            uint16_t ws_col;
            uint16_t ws_xpixel;
            uint16_t ws_ypixel;
        } *ws = (struct winsize *)arg;
        
        if (graphics_is_available()) {
            FramebufferInfo *fb = graphics_get_info();
            ws->ws_col = fb->width / 8;   // 8 pixel font
            ws->ws_row = fb->height / 16; // 16 pixel font
            ws->ws_xpixel = fb->width;
            ws->ws_ypixel = fb->height;
        } else {
            ws->ws_col = 80;
            ws->ws_row = 25;
            ws->ws_xpixel = 640;
            ws->ws_ypixel = 400;
        }
        return 0;
    }
    
    return (uint64_t)-25;  // -ENOTTY
}

// nanosleep(req, rem) - syscall 35
static uint64_t linux_sys_nanosleep(uint64_t req_ptr, uint64_t rem_ptr, uint64_t arg3,
                                    uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)rem_ptr; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    linux_timespec_t *req = (linux_timespec_t *)req_ptr;
    
    // Convert to milliseconds and busy wait
    uint64_t ms = req->tv_sec * 1000 + req->tv_nsec / 1000000;
    
    // Simple delay (not accurate, but works)
    volatile uint64_t count = ms * 100000;
    while (count--) {
        __asm__ volatile("pause");
    }
    
    return 0;
}

// getpid() - syscall 39
static uint64_t linux_sys_getpid(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    return 1;  // Always PID 1
}

// exit(status) - syscall 60
static uint64_t linux_sys_exit(uint64_t status, uint64_t arg2, uint64_t arg3,
                               uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    console_print("\n[Process exited with status ", 0x808080);
    
    char buf[16];
    int i = 0;
    uint64_t n = status;
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
    console_print(buf, 0x808080);
    console_print("]\n", 0x808080);
    
    // Halt (in a real OS, we'd return to scheduler)
    // For now, just return
    return 0;
}

// uname(buf) - syscall 63
static uint64_t linux_sys_uname(uint64_t buf_ptr, uint64_t arg2, uint64_t arg3,
                                uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    linux_utsname_t *buf = (linux_utsname_t *)buf_ptr;
    
    // Helper to copy string
    #define COPY_STR(dst, src) do { \
        const char *s = src; \
        int i = 0; \
        while (*s && i < 64) { dst[i++] = *s++; } \
        dst[i] = '\0'; \
    } while(0)
    
    COPY_STR(buf->sysname, "NBOS");
    COPY_STR(buf->nodename, "nbos");
    COPY_STR(buf->release, "1.0.0");
    COPY_STR(buf->version, "#1 NBOS 1.0.0");
    COPY_STR(buf->machine, "x86_64");
    COPY_STR(buf->domainname, "(none)");
    
    #undef COPY_STR
    
    return 0;
}

// getcwd(buf, size) - syscall 79
static uint64_t linux_sys_getcwd(uint64_t buf_ptr, uint64_t size, uint64_t arg3,
                                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    char *buf = (char *)buf_ptr;
    if (size >= 2) {
        buf[0] = '/';
        buf[1] = '\0';
        return (uint64_t)buf;
    }
    return (uint64_t)-34;  // -ERANGE
}

// sysinfo(info) - syscall 99
static uint64_t linux_sys_sysinfo(uint64_t info_ptr, uint64_t arg2, uint64_t arg3,
                                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    linux_sysinfo_t *info = (linux_sysinfo_t *)info_ptr;
    
    HeapStats stats;
    heap_get_stats(&stats);
    
    info->uptime = system_ticks / 18;  // Approximate seconds
    info->loads[0] = 0;
    info->loads[1] = 0;
    info->loads[2] = 0;
    info->totalram = stats.total_size;
    info->freeram = stats.free_size;
    info->sharedram = 0;
    info->bufferram = 0;
    info->totalswap = 0;
    info->freeswap = 0;
    info->procs = 1;
    info->totalhigh = 0;
    info->freehigh = 0;
    info->mem_unit = 1;
    
    return 0;
}

// getuid() - syscall 102
static uint64_t linux_sys_getuid(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    return 0;  // root
}

// getgid() - syscall 104
static uint64_t linux_sys_getgid(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                                 uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    return 0;  // root
}

// geteuid() - syscall 107
static uint64_t linux_sys_geteuid(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    return 0;  // root
}

// getegid() - syscall 108
static uint64_t linux_sys_getegid(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    return 0;  // root
}

// getppid() - syscall 110
static uint64_t linux_sys_getppid(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                                  uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    return 0;  // init's parent
}

// arch_prctl(code, addr) - syscall 158
static uint64_t linux_sys_arch_prctl(uint64_t code, uint64_t addr, uint64_t arg3,
                                     uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    #define ARCH_SET_FS 0x1002
    #define ARCH_GET_FS 0x1003
    #define ARCH_SET_GS 0x1001
    #define ARCH_GET_GS 0x1004
    
    switch (code) {
        case ARCH_SET_FS:
            // Set FS base (for thread-local storage)
            __asm__ volatile("wrfsbase %0" :: "r"(addr));
            return 0;
        case ARCH_SET_GS:
            // Set GS base
            __asm__ volatile("wrgsbase %0" :: "r"(addr));
            return 0;
        case ARCH_GET_FS:
            __asm__ volatile("rdfsbase %0" : "=r"(*(uint64_t *)addr));
            return 0;
        case ARCH_GET_GS:
            __asm__ volatile("rdgsbase %0" : "=r"(*(uint64_t *)addr));
            return 0;
    }
    
    return (uint64_t)-22;  // -EINVAL
}

// clock_gettime(clockid, tp) - syscall 228
static uint64_t linux_sys_clock_gettime(uint64_t clockid, uint64_t tp_ptr, uint64_t arg3,
                                        uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)clockid; (void)arg3; (void)arg4; (void)arg5; (void)arg6;
    
    linux_timespec_t *tp = (linux_timespec_t *)tp_ptr;
    
    // Return approximate time since boot
    tp->tv_sec = system_ticks / 18;  // ~18.2 Hz PIT
    tp->tv_nsec = (system_ticks % 18) * 55000000;  // ~55ms per tick
    
    return 0;
}

// exit_group(status) - syscall 231
static uint64_t linux_sys_exit_group(uint64_t status, uint64_t arg2, uint64_t arg3,
                                     uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    return linux_sys_exit(status, arg2, arg3, arg4, arg5, arg6);
}

// getrandom(buf, buflen, flags) - syscall 318
static uint64_t linux_sys_getrandom(uint64_t buf_ptr, uint64_t buflen, uint64_t flags,
                                    uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)flags; (void)arg4; (void)arg5; (void)arg6;
    
    uint8_t *buf = (uint8_t *)buf_ptr;
    
    // Simple PRNG (not cryptographically secure!)
    static uint64_t seed = 12345678901234567ULL;
    
    for (uint64_t i = 0; i < buflen; i++) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        buf[i] = (uint8_t)(seed >> 33);
    }
    
    return buflen;
}

/* ============================================================================
 * Syscall table and registration
 * ============================================================================ */

typedef uint64_t (*LinuxSyscallHandler)(uint64_t, uint64_t, uint64_t,
                                         uint64_t, uint64_t, uint64_t);

#define LINUX_MAX_SYSCALL 512
static LinuxSyscallHandler linux_syscall_table[LINUX_MAX_SYSCALL];

void linux_syscall_init(void) {
    // Clear table
    for (int i = 0; i < LINUX_MAX_SYSCALL; i++) {
        linux_syscall_table[i] = 0;
    }
    
    // Initialize FD table
    init_fd_table();
    
    // Register Linux syscalls
    linux_syscall_table[LINUX_SYS_READ] = linux_sys_read;
    linux_syscall_table[LINUX_SYS_WRITE] = linux_sys_write;
    linux_syscall_table[LINUX_SYS_OPEN] = linux_sys_open;
    linux_syscall_table[LINUX_SYS_CLOSE] = linux_sys_close;
    linux_syscall_table[LINUX_SYS_FSTAT] = linux_sys_fstat;
    linux_syscall_table[LINUX_SYS_MMAP] = linux_sys_mmap;
    linux_syscall_table[LINUX_SYS_MUNMAP] = linux_sys_munmap;
    linux_syscall_table[LINUX_SYS_BRK] = linux_sys_brk;
    linux_syscall_table[LINUX_SYS_IOCTL] = linux_sys_ioctl;
    linux_syscall_table[LINUX_SYS_NANOSLEEP] = linux_sys_nanosleep;
    linux_syscall_table[LINUX_SYS_GETPID] = linux_sys_getpid;
    linux_syscall_table[LINUX_SYS_EXIT] = linux_sys_exit;
    linux_syscall_table[LINUX_SYS_UNAME] = linux_sys_uname;
    linux_syscall_table[LINUX_SYS_GETCWD] = linux_sys_getcwd;
    linux_syscall_table[LINUX_SYS_SYSINFO] = linux_sys_sysinfo;
    linux_syscall_table[LINUX_SYS_GETUID] = linux_sys_getuid;
    linux_syscall_table[LINUX_SYS_GETGID] = linux_sys_getgid;
    linux_syscall_table[LINUX_SYS_GETEUID] = linux_sys_geteuid;
    linux_syscall_table[LINUX_SYS_GETEGID] = linux_sys_getegid;
    linux_syscall_table[LINUX_SYS_GETPPID] = linux_sys_getppid;
    linux_syscall_table[LINUX_SYS_ARCH_PRCTL] = linux_sys_arch_prctl;
    linux_syscall_table[LINUX_SYS_CLOCK_GETTIME] = linux_sys_clock_gettime;
    linux_syscall_table[LINUX_SYS_EXIT_GROUP] = linux_sys_exit_group;
    linux_syscall_table[LINUX_SYS_GETRANDOM] = linux_sys_getrandom;
}

int is_linux_syscall(uint64_t num) {
    return num < LINUX_MAX_SYSCALL && linux_syscall_table[num] != 0;
}

void *get_linux_syscall_handler(uint64_t num) {
    if (num < LINUX_MAX_SYSCALL) {
        return (void *)linux_syscall_table[num];
    }
    return 0;
}

// Main dispatcher for Linux syscalls
int64_t linux_syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2,
                              uint64_t arg3, uint64_t arg4, uint64_t arg5,
                              uint64_t arg6) {
    if (num >= LINUX_MAX_SYSCALL || linux_syscall_table[num] == 0) {
        // Unimplemented syscall
        console_print("[Linux] Unimplemented syscall: ", CONSOLE_COLOR_YELLOW);
        
        // Print syscall number
        char buf[20];
        int i = 0;
        uint64_t n = num;
        if (n == 0) {
            buf[i++] = '0';
        } else {
            while (n > 0 && i < 19) {
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
        console_print(buf, CONSOLE_COLOR_YELLOW);
        console_print("\n", CONSOLE_COLOR_YELLOW);
        
        return (int64_t)-38;  // -ENOSYS
    }
    
    LinuxSyscallHandler handler = linux_syscall_table[num];
    return (int64_t)handler(arg1, arg2, arg3, arg4, arg5, arg6);
}

// Called by timer interrupt to track uptime
void linux_syscall_tick(void) {
    system_ticks++;
}
