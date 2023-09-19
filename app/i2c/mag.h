#ifndef MAG_H__
#define MAG_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	int16_t mag_field[3];
	bool sensor_present;
} mag_t;

int mag_init(void);
void mag_poll(uint32_t diff_ms);

extern mag_t mag_data;

#endif // MAG_H__