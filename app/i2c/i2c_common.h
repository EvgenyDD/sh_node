#ifndef I2C_COMMON_H__
#define I2C_COMMON_H__

#include <stdbool.h>
#include <stdint.h>

void i2c_common_init(void);
int i2c_common_read(uint8_t addr, uint8_t *data, uint16_t size);
int i2c_common_write_to_read(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t size);
int i2c_common_write(uint8_t addr, uint8_t reg, const uint8_t *data, uint16_t size);
int i2c_common_check_addr(uint8_t addr);

int HAL_I2C_Mem_Write(uint16_t DevAddress, uint16_t MemAddress, bool is_mem_16_bit, const uint8_t *pData, uint16_t Size);
int HAL_I2C_Mem_Read(uint16_t DevAddress, uint16_t MemAddress, bool is_mem_16_bit, uint8_t *pData, uint16_t Size);
int HAL_I2C_IsDeviceReady(uint16_t DevAddress, uint32_t Trials);

#endif // I2C_COMMON_H__