#ifndef EEPROM_H__
#define EEPROM_H__

#include <stdint.h>

#define EEPROM_SIZE 8192

enum
{
	EEP_ERR_ADDR_OVF = -100,
};

int eeprom_read(uint32_t addr, uint8_t *data, uint32_t size);
int eeprom_write(uint32_t addr, const uint8_t *data, uint32_t size);

#endif // EEPROM_H__