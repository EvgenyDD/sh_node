#include "lib.h"

void _memcpy(void *dest, const void *src, size_t n) // lower size while compiling in linux
{
	const char *csrc = (const char *)src;
	char *cdest = (char *)dest;
	for(volatile size_t i = 0; i < n; i++)
		cdest[i] = csrc[i];
}