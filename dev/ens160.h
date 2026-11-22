#ifndef ENS160_H__
#define ENS160_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	bool exist;
	uint8_t addr;

	uint8_t status;
	uint8_t aqi;
	uint16_t tvoc;
	uint16_t eco2;

	uint32_t err_cnt;
} ens160_t;

void ens160_init(void);
void ens160_poll(uint32_t diff_ms);

int ens160_set_pwr_mode(uint8_t mode);
int ens160_set_int_mode(uint8_t mode);
int ens160_get_status(uint8_t *sts);
int ens160_get_aqi(uint8_t *aqi);
int ens160_get_tvoc(uint16_t *tvoc);
int ens160_get_eco2(uint16_t *eco2);
int ens160_set_temp_hum(uint16_t temp, uint16_t rh);

extern ens160_t ens160;

#endif // ENS160_H__