#pragma once

typedef signed char        int8_t;
typedef unsigned char      uint8_t;
typedef signed short       int16_t;
typedef unsigned short     uint16_t;
typedef signed int         int32_t;
typedef unsigned int       uint32_t;
typedef signed long long   int64_t;
typedef unsigned long long uint64_t;

// Pointer-sized integers (64-bit in long mode)
typedef int64_t            intptr_t;
typedef uint64_t           uintptr_t;
typedef uint64_t           size_t;

typedef uint8_t bool;

#define false 0
#define true 1

#define NULL ((void*)0)
