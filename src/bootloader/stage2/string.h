#pragma once

// Return the first occurrence of chr in str, or NULL when absent.
const char* strchr(const char* str, char chr);

// Copy src, including its terminator, into dst.
char* strcpy(char* dst, const char* src);

// Return the length of a null-terminated string.
unsigned strlen(const char* str);
