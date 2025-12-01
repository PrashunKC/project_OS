/*
 * NBOS System API Header
 * 
 * This header provides the core system functions for NBOS programs.
 * Include this header in all NBOS programs.
 */

#ifndef _NBOS_H
#define _NBOS_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

typedef uint64_t size_t;
typedef int64_t  ssize_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

/* ============================================================================
 * System Calls
 * ============================================================================ */

// System call numbers
#define SYS_EXIT      0
#define SYS_PRINT     1
#define SYS_GETKEY    2
#define SYS_KBHIT     3
#define SYS_MALLOC    4
#define SYS_FREE      5
#define SYS_SLEEP     6
#define SYS_PUTPIXEL  10
#define SYS_GETPIXEL  11
#define SYS_CLEAR     12
#define SYS_GETWIDTH  13
#define SYS_GETHEIGHT 14

// Inline system call wrapper
static inline uint64_t syscall0(uint64_t num) {
    uint64_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num));
    return ret;
}

static inline uint64_t syscall1(uint64_t num, uint64_t arg1) {
    uint64_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1));
    return ret;
}

static inline uint64_t syscall2(uint64_t num, uint64_t arg1, uint64_t arg2) {
    uint64_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2));
    return ret;
}

static inline uint64_t syscall3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    uint64_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3));
    return ret;
}

static inline uint64_t syscall4(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    uint64_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4));
    return ret;
}

/* ============================================================================
 * Console I/O
 * ============================================================================ */

// Print a string to the console
static inline void print(const char *str) {
    syscall1(SYS_PRINT, (uint64_t)str);
}

// Get a keypress (blocking)
static inline char getkey(void) {
    return (char)syscall0(SYS_GETKEY);
}

// Check if a key is available (non-blocking)
static inline int kbhit(void) {
    return (int)syscall0(SYS_KBHIT);
}

/* ============================================================================
 * Memory Management
 * ============================================================================ */

// Allocate memory
static inline void *malloc(size_t size) {
    return (void *)syscall1(SYS_MALLOC, size);
}

// Free memory
static inline void free(void *ptr) {
    syscall1(SYS_FREE, (uint64_t)ptr);
}

// Set memory to a value
static inline void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    while (n--) {
        *p++ = (uint8_t)c;
    }
    return s;
}

// Copy memory
static inline void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

/* ============================================================================
 * System Control
 * ============================================================================ */

// Exit the program
static inline void exit(int code) {
    syscall1(SYS_EXIT, (uint64_t)code);
    for(;;); // Never returns
}

// Sleep for milliseconds
static inline void sleep(int ms) {
    syscall1(SYS_SLEEP, (uint64_t)ms);
}

/* ============================================================================
 * String Functions
 * ============================================================================ */

static inline size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

static inline int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static inline char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

static inline char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

#ifdef __cplusplus
}
#endif

#endif /* _NBOS_H */
