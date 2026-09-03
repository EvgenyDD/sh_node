#ifndef I2C_COMMON_H__
#define I2C_COMMON_H__

#include <stdbool.h>
#include <stdint.h>

enum
{
	I2C_ERR_TIMEOUT = -3,
	I2C_ERR_BUSY = -2,
	I2C_ERR_GENERAL = -1,
};

void i2c_init(void);

int i2c_mem_write(uint16_t DevAddress, uint16_t MemAddress, bool is_mem_16_bit, const uint8_t *pData, uint16_t Size);
int i2c_mem_read(uint16_t DevAddress, uint16_t MemAddress, bool is_mem_16_bit, uint8_t *pData, uint16_t Size);
int i2c_is_device_ready(uint16_t DevAddress, uint32_t Trials);
int i2c_tx(uint16_t DevAddress, uint8_t *pData, uint16_t Size);
int i2c_rx(uint16_t DevAddress, uint8_t *pData, uint16_t Size);

#endif // I2C_COMMON_H__