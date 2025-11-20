#include "stdio.h"
#include "x86.h"
#include <stdarg.h>

void putc(char c)
{
    x86_Video_WriteCharTeletype(c, 0);
}

void puts(const char* str)
{
    while (*str)
    {
        putc(*str);
        str++;
    }
}

const char g_HexChars[] = "0123456789abcdef";

static void printf_unsigned32(uint32_t number, uint32_t radix)
{
    char buffer[32];
    int pos = 0;

    do
    {
        uint32_t rem = number % radix;
        number /= radix;
        buffer[pos++] = g_HexChars[rem & 0xF];
    }
    while(number > 0);

    while(--pos >= 0)
    {
        putc(buffer[pos]);
    }
}

static void printf_signed32(int32_t number, int radix)
{
    if (number < 0)
    {
        putc('-');
        printf_unsigned32((uint32_t)(-number), (uint32_t)radix);
    }
    else
    {
        printf_unsigned32((uint32_t)number, (uint32_t)radix);
    }
}

void _cdecl printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    while(*fmt)
    {
        if(*fmt == '%')
        {
            fmt++;
            int length = 0; // 0=default, 1=short, 2=long, 3=long long
            
            if(*fmt == 'h') { length = 1; fmt++; if(*fmt == 'h') { length = 1; fmt++; } }
            else if(*fmt == 'l') { length = 2; fmt++; if(*fmt == 'l') { length = 3; fmt++; } }

            switch(*fmt)
            {
                case 'c': putc((char)va_arg(args, int)); break;
                case 's': puts(va_arg(args, const char*)); break;
                case '%': putc('%'); break;
                case 'd':
                case 'i': 
                {
                    int32_t value;
                    if (length == 3)
                    {
                        value = (int32_t)va_arg(args, long long);
                    }
                    else
                    {
                        value = (int32_t)va_arg(args, int);
                    }
                    printf_signed32(value, 10);
                    break;
                }
                case 'u': 
                {
                    uint32_t value;
                    if (length == 3)
                    {
                        value = (uint32_t)va_arg(args, unsigned long long);
                    }
                    else
                    {
                        value = (uint32_t)va_arg(args, unsigned int);
                    }
                    printf_unsigned32(value, 10);
                    break;
                }
                case 'x':
                case 'X':
                case 'p': 
                {
                    uint32_t value;
                    if (length == 3)
                    {
                        value = (uint32_t)va_arg(args, unsigned long long);
                    }
                    else
                    {
                        value = (uint32_t)va_arg(args, unsigned int);
                    }
                    printf_unsigned32(value, 16);
                    break;
                }
                default: putc('%'); putc(*fmt); break;
            }
        }
        else
        {
            putc(*fmt);
        }
        fmt++;
    }
    
    va_end(args);
}
