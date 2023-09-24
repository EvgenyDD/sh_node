#include "aht21.h"
#include "CANopen.h"
#include "i2c_common.h"
#include "OD.h"
#include <stdbool.h>
#include <stddef.h>

#define POLL_HALF_INTERVAL 3000

extern void delay_ms(volatile uint32_t delay_ms);
aht21_t aht21_data = {0};

#define AHT21_ADDR 0x38

#define AHT21_CMD_TRIG_MEAS 0xAC
#define AHT21_CMD_RESET 0xBA
#define AHT21_CMD_INIT 0xBE

static bool is_wait_conv = false;
static uint32_t tmr_poll = 0;

int aht21_read_status(uint8_t *status) { return i2c_common_read(AHT21_ADDR, status, 1); }
int aht21_reset(void) { return i2c_common_write(AHT21_ADDR, AHT21_CMD_RESET, NULL, 0); }
int aht21_is_present(void) { return i2c_common_check_addr(AHT21_ADDR); }

int aht21_init(void)
{
	delay_ms(40);

	if(aht21_is_present())
	{
		aht21_data.sensor_present = false;
		return 1;
	}

	aht21_data.sensor_present = true;

	uint8_t status;
	int sts = aht21_read_status(&status);
	if(sts) return sts;
	if((status & (1 << 3)) != (1 << 3))
	{
		sts = i2c_common_write(AHT21_ADDR, AHT21_CMD_INIT, (uint8_t[]){0x08, 0x00}, 2);
		delay_ms(10);
	}
	// return aht21_reset();

	return aht21_read_status(&status);
}

int aht21_poll(uint32_t diff_ms)
{
	tmr_poll += diff_ms;
	if(tmr_poll >= POLL_HALF_INTERVAL)
	{
		tmr_poll = 0;
		if(is_wait_conv == false)
		{
			is_wait_conv = true;
			int sts = i2c_common_write(AHT21_ADDR, AHT21_CMD_TRIG_MEAS, (uint8_t[]){0x33, 0x00}, 2);
			return sts;
		}

		uint8_t data[7];
		int sts = i2c_common_read(AHT21_ADDR, data, 7);
		if(sts) return sts;

		if((data[0] & (1 << 7)) == 0)
		{
			is_wait_conv = false;
			uint32_t var = ((data[1] << 16) | (data[2] << 8) | data[3]) >> 4;
			aht21_data.hum_0_1perc = (var * 125) >> 17;
			OD_RAM.x6103_aht21.hum = aht21_data.hum_0_1perc;

			var = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];
			aht21_data.temp_0_1C = ((var * 125) >> 16) - 500;
			OD_RAM.x6103_aht21.temp = aht21_data.temp_0_1C;
		}
		return sts;
	}
	return 0;
}
