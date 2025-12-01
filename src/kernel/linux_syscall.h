/*
 * Linux Syscall Compatibility Layer for NBOS
 * 
 * This provides Linux x86_64 syscall numbers and implementations
 * to allow Linux programs to run on NBOS with minimal modifications.
 * 
 * Reference: Linux kernel source (arch/x86/entry/syscalls/syscall_64.tbl)
 */

#ifndef _LINUX_SYSCALL_H
#define _LINUX_SYSCALL_H

#include "stdint.h"

/* ============================================================================
 * Linux x86_64 Syscall Numbers
 * These are the official Linux syscall numbers for x86_64
 * ============================================================================ */

// File operations
#define LINUX_SYS_READ          0
#define LINUX_SYS_WRITE         1
#define LINUX_SYS_OPEN          2
#define LINUX_SYS_CLOSE         3
#define LINUX_SYS_STAT          4
#define LINUX_SYS_FSTAT         5
#define LINUX_SYS_LSTAT         6
#define LINUX_SYS_POLL          7
#define LINUX_SYS_LSEEK         8
#define LINUX_SYS_MMAP          9
#define LINUX_SYS_MPROTECT      10
#define LINUX_SYS_MUNMAP        11
#define LINUX_SYS_BRK           12
#define LINUX_SYS_RT_SIGACTION  13
#define LINUX_SYS_RT_SIGPROCMASK 14
#define LINUX_SYS_RT_SIGRETURN  15
#define LINUX_SYS_IOCTL         16
#define LINUX_SYS_PREAD64       17
#define LINUX_SYS_PWRITE64      18
#define LINUX_SYS_READV         19
#define LINUX_SYS_WRITEV        20
#define LINUX_SYS_ACCESS        21
#define LINUX_SYS_PIPE          22
#define LINUX_SYS_SELECT        23
#define LINUX_SYS_SCHED_YIELD   24
#define LINUX_SYS_MREMAP        25
#define LINUX_SYS_MSYNC         26
#define LINUX_SYS_MINCORE       27
#define LINUX_SYS_MADVISE       28
#define LINUX_SYS_SHMGET        29
#define LINUX_SYS_SHMAT         30
#define LINUX_SYS_SHMCTL        31
#define LINUX_SYS_DUP           32
#define LINUX_SYS_DUP2          33
#define LINUX_SYS_PAUSE         34
#define LINUX_SYS_NANOSLEEP     35
#define LINUX_SYS_GETITIMER     36
#define LINUX_SYS_ALARM         37
#define LINUX_SYS_SETITIMER     38
#define LINUX_SYS_GETPID        39
#define LINUX_SYS_SENDFILE      40

// Socket operations (41-55)
#define LINUX_SYS_SOCKET        41
#define LINUX_SYS_CONNECT       42
#define LINUX_SYS_ACCEPT        43
#define LINUX_SYS_SENDTO        44
#define LINUX_SYS_RECVFROM      45
#define LINUX_SYS_SENDMSG       46
#define LINUX_SYS_RECVMSG       47
#define LINUX_SYS_SHUTDOWN      48
#define LINUX_SYS_BIND          49
#define LINUX_SYS_LISTEN        50
#define LINUX_SYS_GETSOCKNAME   51
#define LINUX_SYS_GETPEERNAME   52
#define LINUX_SYS_SOCKETPAIR    53
#define LINUX_SYS_SETSOCKOPT    54
#define LINUX_SYS_GETSOCKOPT    55

// Process control
#define LINUX_SYS_CLONE         56
#define LINUX_SYS_FORK          57
#define LINUX_SYS_VFORK         58
#define LINUX_SYS_EXECVE        59
#define LINUX_SYS_EXIT          60
#define LINUX_SYS_WAIT4         61
#define LINUX_SYS_KILL          62
#define LINUX_SYS_UNAME         63

// More file operations
#define LINUX_SYS_FCNTL         72
#define LINUX_SYS_FLOCK         73
#define LINUX_SYS_FSYNC         74
#define LINUX_SYS_FDATASYNC     75
#define LINUX_SYS_TRUNCATE      76
#define LINUX_SYS_FTRUNCATE     77
#define LINUX_SYS_GETDENTS      78
#define LINUX_SYS_GETCWD        79
#define LINUX_SYS_CHDIR         80
#define LINUX_SYS_FCHDIR        81
#define LINUX_SYS_RENAME        82
#define LINUX_SYS_MKDIR         83
#define LINUX_SYS_RMDIR         84
#define LINUX_SYS_CREAT         85
#define LINUX_SYS_LINK          86
#define LINUX_SYS_UNLINK        87
#define LINUX_SYS_SYMLINK       88
#define LINUX_SYS_READLINK      89
#define LINUX_SYS_CHMOD         90
#define LINUX_SYS_FCHMOD        91
#define LINUX_SYS_CHOWN         92
#define LINUX_SYS_FCHOWN        93
#define LINUX_SYS_LCHOWN        94

// User/Group IDs
#define LINUX_SYS_GETUID        102
#define LINUX_SYS_GETGID        104
#define LINUX_SYS_GETEUID       107
#define LINUX_SYS_GETEGID       108
#define LINUX_SYS_GETPPID       110
#define LINUX_SYS_GETPGRP       111

// Time
#define LINUX_SYS_GETTIMEOFDAY  96
#define LINUX_SYS_GETRLIMIT     97
#define LINUX_SYS_GETRUSAGE     98
#define LINUX_SYS_SYSINFO       99
#define LINUX_SYS_TIMES         100
#define LINUX_SYS_CLOCK_GETTIME 228
#define LINUX_SYS_CLOCK_NANOSLEEP 230

// Misc
#define LINUX_SYS_ARCH_PRCTL    158
#define LINUX_SYS_EXIT_GROUP    231
#define LINUX_SYS_OPENAT        257
#define LINUX_SYS_NEWFSTATAT    262
#define LINUX_SYS_READLINKAT    267
#define LINUX_SYS_GETRANDOM     318

/* ============================================================================
 * File descriptor constants
 * ============================================================================ */
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

/* ============================================================================
 * Open flags (O_*)
 * ============================================================================ */
#define O_RDONLY        0x0000
#define O_WRONLY        0x0001
#define O_RDWR          0x0002
#define O_CREAT         0x0040
#define O_EXCL          0x0080
#define O_NOCTTY        0x0100
#define O_TRUNC         0x0200
#define O_APPEND        0x0400
#define O_NONBLOCK      0x0800
#define O_DIRECTORY     0x10000
#define O_CLOEXEC       0x80000

/* ============================================================================
 * Memory protection flags
 * ============================================================================ */
#define PROT_NONE       0x0
#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4

#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_FAILED      ((void *)-1)

/* ============================================================================
 * Structures
 * ============================================================================ */

// Linux timespec structure
typedef struct {
    int64_t tv_sec;     // seconds
    int64_t tv_nsec;    // nanoseconds
} linux_timespec_t;

// Linux timeval structure  
typedef struct {
    int64_t tv_sec;     // seconds
    int64_t tv_usec;    // microseconds
} linux_timeval_t;

// Linux stat structure (simplified)
typedef struct {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    linux_timespec_t st_atim;
    linux_timespec_t st_mtim;
    linux_timespec_t st_ctim;
    int64_t  __unused[3];
} linux_stat_t;

// Linux utsname structure
typedef struct {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
} linux_utsname_t;

// Linux sysinfo structure
typedef struct {
    int64_t uptime;
    uint64_t loads[3];
    uint64_t totalram;
    uint64_t freeram;
    uint64_t sharedram;
    uint64_t bufferram;
    uint64_t totalswap;
    uint64_t freeswap;
    uint16_t procs;
    uint16_t pad;
    uint64_t totalhigh;
    uint64_t freehigh;
    uint32_t mem_unit;
    char _f[20-2*sizeof(uint64_t)-sizeof(uint32_t)];
} linux_sysinfo_t;

/* ============================================================================
 * Function declarations
 * ============================================================================ */

// Initialize Linux syscall compatibility layer
void linux_syscall_init(void);

// Check if a syscall number is a Linux syscall
int is_linux_syscall(uint64_t num);

// Get Linux syscall handler
void *get_linux_syscall_handler(uint64_t num);

// Main Linux syscall dispatcher
// Takes syscall number and 6 arguments, returns result
int64_t linux_syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2,
                              uint64_t arg3, uint64_t arg4, uint64_t arg5,
                              uint64_t arg6);

#endif /* _LINUX_SYSCALL_H */
