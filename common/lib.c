#include "lib.h"

void _memcpy(void *dest, const void *src, size_t n)
{
    const char *csrc = (const char *)src;
    char *cdest = (char *)dest;
    for (volatile int i=0; i<n; i++) cdest[i] = csrc[i];
}