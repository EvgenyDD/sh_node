#ifndef TACHO_H__
#define TACHO_H__

#include <stdint.h>

void tacho_init(void);
void tacho_poll(uint32_t diff_ms);

#endif // TACHO_H__