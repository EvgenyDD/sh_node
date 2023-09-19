#ifndef SPI_COMMON_H__
#define SPI_COMMON_H__

#include <stdint.h>

void spi_common_init(void);
void spi_trx(const uint8_t *data_tx, uint8_t *data_rx, uint32_t sz);

#endif // SPI_COMMON_H__