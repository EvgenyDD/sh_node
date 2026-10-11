#ifndef PIR_H
#define PIR_H

#include <stdint.h>

void pir_init(void);
void pir_poll(uint32_t diff_ms, uint8_t pdo_n);

#endif // PIR_H