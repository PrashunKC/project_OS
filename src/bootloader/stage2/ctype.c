// ASCII character classification and case conversion for the freestanding loader.
#include "ctype.h"

bool isLower(char chr)
{
    return (chr >= 'a' && chr <= 'z');
}

char toupper(char chr)
{
    return isLower(chr) ? (chr - 'a' + 'A') : chr;
}
