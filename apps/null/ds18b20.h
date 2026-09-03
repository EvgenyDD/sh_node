#ifndef DS18B20_H__
#define DS18B20_H__

#include <stdint.h>

void ds18b20_init(uint32_t baudrate);
int ds18b20_read(uint32_t diff_ms);

#endif // DS18B20_H__
