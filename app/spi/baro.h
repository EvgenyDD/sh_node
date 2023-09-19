#ifndef BARO_H__
#define BARO_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	int32_t temp, temp_raw;
	int32_t pres, pres_raw;
	bool sensor_present;
} baro_t;

int baro_init(void);
void baro_poll(uint32_t diff_ms);

extern baro_t baro_data;

#endif // BARO_H__