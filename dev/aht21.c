#include "aht21.h"
#include "CANopen.h"
#include "OD.h"
#include "i2c_common.h"
#include <stdbool.h>
#include <stddef.h>

#define POLL_HALF_INTERVAL 3000

extern void delay_ms(volatile uint32_t delay_ms);
aht21_t aht21 = {0};

#define AHT21_ADDR (0x38 << 1)

#define AHT21_CMD_TRIG_MEAS 0xAC
#define AHT21_CMD_RESET 0xBA
#define AHT21_CMD_INIT 0xBE

static bool is_wait_conv = false;
static uint32_t tmr_poll = 0;

int aht21_read_status(uint8_t *status) { return i2c_rx(AHT21_ADDR, status, 1); }
int aht21_reset(void) { return i2c_tx(AHT21_ADDR, (uint8_t[]){AHT21_CMD_RESET}, 1); }

int aht21_is_present(void)
{
	uint8_t sts;
	return aht21_read_status(&sts);
}

static void reset_reg(uint8_t reg)
{
	i2c_mem_write(AHT21_ADDR, reg, false, (uint8_t[]){0x00, 0x00}, 2);
	delay_ms(5);

	uint8_t data[3];
	i2c_rx(AHT21_ADDR, data, 3);
	delay_ms(10);

	i2c_mem_write(AHT21_ADDR, 0xB0 | reg, false, (uint8_t[]){data[1], data[2]}, 2);
}

int aht21_init(void)
{
	delay_ms(40);

	if(aht21_is_present())
	{
		aht21.exist = false;
		return 1;
	}

	aht21.exist = true;

	aht21_reset();
	delay_ms(40);

	uint8_t status;
	int sts = aht21_read_status(&status);
	if(sts) return sts;
	if((status & (1 << 3)) != (1 << 3))
	{
		sts = i2c_mem_write(AHT21_ADDR, 0xA8, false, (uint8_t[]){0x00, 0x00}, 2);
		sts = i2c_mem_write(AHT21_ADDR, AHT21_CMD_INIT, false, (uint8_t[]){0x08, 0x00}, 2);
		delay_ms(10);
	}
	if((status & 0x18) != 0x18)
	{
		reset_reg(0x1b);
		reset_reg(0x1c);
		reset_reg(0x1e);
	}

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
			int sts = i2c_mem_write(AHT21_ADDR, AHT21_CMD_TRIG_MEAS, false, (uint8_t[]){0x33, 0x00}, 2);
			return sts;
		}

		uint8_t data[7];
		int sts = i2c_rx(AHT21_ADDR, data, 7);
		if(sts) return sts;

		if((data[0] & (1 << 7)) == 0)
		{
			is_wait_conv = false;
			uint32_t var = (uint32_t)((data[1] << 16UL) | (data[2] << 8UL) | data[3]) >> 4UL;
			aht21.hum_0_1perc = (int32_t)((var * 125u) >> 17u);
			OD_RAM.x6103_aht21.hum = aht21.hum_0_1perc;

			var = (uint32_t)((data[3] & 0x0F) << 16u) | (data[4] << 8u) | data[5];
			aht21.temp_0_1C = (int32_t)var * 200 * 10 / 1024 / 1024 - 500;
			OD_RAM.x6103_aht21.temp = aht21.temp_0_1C;
		}
		return sts;
	}
	return 0;
}
