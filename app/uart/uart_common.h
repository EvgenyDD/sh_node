#ifndef UART_COMMON_H__
#define UART_COMMON_H__

#include <stdint.h>

void uart_common_init(void);

void uart_tx(const uint8_t *data, uint32_t len);

#endif // UART_COMMON_H__