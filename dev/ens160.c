#include "ens160.h"
#include "CANopen.h"
#include "OD.h"
#include "aht21.h"
#include "cfg_device.h"
#include "i2c_common.h"
#include "platform.h"

#ifdef CFG_USE_ENS160

#define POLL_INTERVAL 500

#define ENS160_ADDR_1 0xA4 // if ADD=0
#define ENS160_ADDR_2 0xA6 // if ADD=1

#define REG_PART_ID 0x0

#define REG_OPMODE 0x10
#define REG_CONFIG 0x11
#define REG_COMMAND 0x12
#define REG_TEMP_IN 0x13
#define REG_RH_IN 0x15
#define REG_DEVICE_STATUS 0x20
#define REG_DATA_AQI 0x21
#define REG_DATA_TVOC 0x22
#define REG_DATA_ECO2 0x24
#define REG_DATA_T 0x30
#define REG_DATA_RH 0x32
#define REG_DATA_MISR 0x38
#define REG_GPR_WR 0x40
#define REG_GPR_RD 0x48

#define MODE_SLEEP 0x00
#define MODE_IDLE 0x01
#define MODE_STANDARD 0x02

ens160_t ens160 = {0};

static uint32_t tmr_poll = 0;

int ens160_set_pwr_mode(uint8_t mode)
{
	tmr_poll = 0;
	return i2c_mem_write(ens160.addr, REG_OPMODE, false, &mode, 1);
}

int ens160_set_int_mode(uint8_t mode)
{
	tmr_poll = 0;
	return i2c_mem_write(ens160.addr, REG_CONFIG, false, &mode, 1);
}

int ens160_get_status(uint8_t *sts)
{
	return i2c_mem_read(ens160.addr, REG_DEVICE_STATUS, false, sts, 1);
}

int ens160_get_aqi(uint8_t *aqi)
{
	return i2c_mem_read(ens160.addr, REG_DATA_AQI, false, aqi, 1);
}

int ens160_get_tvoc(uint16_t *tvoc)
{
	uint8_t buf[2];
	int sts = i2c_mem_read(ens160.addr, REG_DATA_TVOC, false, buf, 2);
	if(!sts) *tvoc = ((uint16_t)buf[1] << 8) | (uint16_t)buf[0];
	return sts;
}

int ens160_get_eco2(uint16_t *eco2)
{
	uint8_t buf[2];
	int sts = i2c_mem_read(ens160.addr, REG_DATA_ECO2, false, buf, 2);
	if(!sts) *eco2 = ((uint16_t)buf[1] << 8) | (uint16_t)buf[0];
	return sts;
}

int ens160_set_temp_hum(uint16_t temp, uint16_t rh)
{
	uint8_t buf[4];

	buf[0] = temp & 0xFF;
	buf[1] = (temp & 0xFF00) >> 8;
	buf[2] = rh & 0xFF;
	buf[3] = (rh & 0xFF00) >> 8;
	return i2c_mem_write(ens160.addr, REG_TEMP_IN, false, buf, sizeof(buf));
}

void ens160_init(void)
{
	uint8_t id[2] = {0};
	ens160.addr = ENS160_ADDR_1;
	if(i2c_mem_read(ens160.addr, REG_PART_ID, false, id, 2))
	{
		ens160.addr = ENS160_ADDR_2;
		if(i2c_mem_read(ens160.addr, REG_PART_ID, false, id, 2)) return;
	}
	if(id[0] != 0x60 || id[1] != 0x01)
	{
		ens160.addr = ENS160_ADDR_2;
		if(i2c_mem_read(ens160.addr, REG_PART_ID, false, id, 2)) return;
	}

	ens160.exist = true;
	ens160_set_pwr_mode(MODE_STANDARD);
	ens160_set_int_mode(0x00);
}

void ens160_poll(uint32_t diff_ms)
{
	tmr_poll += diff_ms;
	if(tmr_poll >= POLL_INTERVAL)
	{
		tmr_poll = 0;

		if(ens160_get_status(&ens160.status))
		{
			ens160.err_cnt++;
		}
		else
		{
			if(ens160.status & (1 << 1))
			{
				if(ens160_get_aqi(&ens160.aqi)) ens160.err_cnt++;
				if(ens160_get_tvoc(&ens160.tvoc)) ens160.err_cnt++;
				if(ens160_get_eco2(&ens160.eco2)) ens160.err_cnt++;
				OD_RAM.x6108_ens160.aqi = ens160.aqi;
				OD_RAM.x6108_ens160.tvoc = ens160.tvoc;
				OD_RAM.x6108_ens160.eco2 = ens160.eco2;
			}
			OD_RAM.x6108_ens160.status = ens160.status;
		}

		if(aht21.exist)
		{
			// uint16_t temp = (aht21.temp_0_1C / 10 + 273.15) * 64;
			// uint16_t rh = aht21.hum_0_1perc / 10 * 512;
			uint16_t temp = ((aht21.temp_0_1C + 2732) * 32) / 5;
			uint16_t rh = (aht21.hum_0_1perc * 256) / 5;
			if(ens160_set_temp_hum(temp, rh)) ens160.err_cnt++;
		}
	}
}

#endif