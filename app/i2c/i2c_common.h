#ifndef I2C_COMMON_H__
#define I2C_COMMON_H__

#include <stdint.h>

void i2c_common_init(void);
int i2c_common_read(uint8_t addr, uint8_t *data, uint16_t size);
int i2c_common_write_to_read(uint8_t addr, uint16_t reg, uint8_t *data, uint16_t size);
int i2c_common_write(uint8_t addr, uint16_t reg, const uint8_t *data, uint16_t size);
int i2c_common_check_addr(uint8_t addr);

#endif // I2C_COMMON_H__