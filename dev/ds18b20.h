#ifndef DS18B20_H__
#define DS18B20_H__

#include <stdint.h>

void ds18b20_init(uint32_t baudrate);
void ds18b20_poll(uint32_t diff_ms);

uint8_t ds18b20_detect(void);

#endif // DS18B20_H__
