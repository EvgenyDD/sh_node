#ifndef AHT21_H__
#define AHT21_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	int32_t temp_0_1C;
	int32_t hum_0_1perc;
	bool sensor_present;
} aht21_t;

int aht21_init(void);
int aht21_reset(void);
int aht21_is_present(void);

int aht21_read_status(uint8_t *status);
int aht21_poll(uint32_t diff_ms);

extern aht21_t aht21_data;

#endif // AHT21_H__