#include "baro.h"
#include "CANopen.h"
#include "OD.h"
#include "spi_common.h"
#include "stm32f10x.h"
#include <string.h>

extern void delay_ms(volatile uint32_t delay_ms);

#define POLL_INTERVAL 5000

#define BMP280_CHIP_ID 0xD0
#define BMP280_RST_REG 0xE0
#define BMP280_STAT_REG 0xF3
#define BMP280_CTRL_MEAS_REG 0xF4
#define BMP280_CONFIG_REG 0xF5
#define BMP280_PRESSURE_MSB_REG 0xF7
#define BMP280_PRESSURE_LSB_REG 0xF8
#define BMP280_PRESSURE_XLSB_REG 0xF9
#define BMP280_TEMPERATURE_MSB_REG 0xFA
#define BMP280_TEMPERATURE_LSB_REG 0xFB
#define BMP280_TEMPERATURE_XLSB_REG 0xFC

#define BMP280_SLEEP_MODE 0x00
#define BMP280_FORCED_MODE 0x01
#define BMP280_NORMAL_MODE 0x03

#define BMP280_TEMPERATURE_CALIB_DIG_T1_LSB_REG 0x88
#define BMP280_PRESSURE_TEMPERATURE_CALIB_DATA_LENGTH 24
#define BMP280_DATA_FRAME_SIZE 6

#define BMP280_OVERSAMP_SKIPPED 0x00
#define BMP280_OVERSAMP_1X 0x01
#define BMP280_OVERSAMP_2X 0x02
#define BMP280_OVERSAMP_4X 0x03
#define BMP280_OVERSAMP_8X 0x04
#define BMP280_OVERSAMP_16X 0x05

#define CALIB_SIZE 24

#define CS_H GPIOA->BSRR = (1 << 15)
#define CS_L GPIOA->BRR = (1 << 15)

baro_t baro_data = {0};

static uint32_t tmr_poll = 0;

static struct
{
	uint16_t dig_T1;
	int16_t dig_T2;
	int16_t dig_T3;
	uint16_t dig_P1;
	int16_t dig_P2;
	int16_t dig_P3;
	int16_t dig_P4;
	int16_t dig_P5;
	int16_t dig_P6;
	int16_t dig_P7;
	int16_t dig_P8;
	int16_t dig_P9;
	int32_t t_fine;
} bmp280_cal_data;

int baro_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStruct);
	CS_H;

	delay_ms(2);
	uint8_t data_rx[32];

	CS_L;
	spi_trx((uint8_t[]){BMP280_CHIP_ID, 0}, data_rx, 2);
	CS_H;

	baro_data.sensor_present = false;
	if(data_rx[1] != 0x58) return 1;
	baro_data.sensor_present = true;

	uint8_t data_tx[1 + CALIB_SIZE] = {BMP280_TEMPERATURE_CALIB_DIG_T1_LSB_REG};
	delay_ms(1);
	CS_L;
	spi_trx(data_tx, data_rx, 1 + CALIB_SIZE);
	CS_H;
	memcpy(&bmp280_cal_data, &data_rx[1], CALIB_SIZE);

	CS_L;
	spi_trx((uint8_t[]){BMP280_CTRL_MEAS_REG & ~(1 << 7), BMP280_OVERSAMP_2X << 2 | BMP280_OVERSAMP_16X << 5 | BMP280_NORMAL_MODE}, data_rx, 2);
	CS_H;

	CS_L;
	spi_trx((uint8_t[]){BMP280_CONFIG_REG & ~(1 << 7), 5 << 2}, data_rx, 2);
	CS_H;

	return 0;
}

static int32_t bmp280CompensateT(int32_t adcT)
{
	int32_t var1, var2, T;
	var1 = ((((adcT >> 3) - ((s32)bmp280_cal_data.dig_T1 << 1))) * ((s32)bmp280_cal_data.dig_T2)) >> 11;
	var2 = (((((adcT >> 4) - ((s32)bmp280_cal_data.dig_T1)) * ((adcT >> 4) - ((s32)bmp280_cal_data.dig_T1))) >> 12) * ((s32)bmp280_cal_data.dig_T3)) >> 14;
	bmp280_cal_data.t_fine = var1 + var2;
	T = (bmp280_cal_data.t_fine * 5 + 128) >> 8;
	return T;
}

static uint32_t bmp280CompensateP(int32_t adcP)
{
	int64_t var1, var2, p;
	var1 = ((int64_t)bmp280_cal_data.t_fine) - 128000;
	var2 = var1 * var1 * (int64_t)bmp280_cal_data.dig_P6;
	var2 = var2 + ((var1 * (int64_t)bmp280_cal_data.dig_P5) << 17);
	var2 = var2 + (((int64_t)bmp280_cal_data.dig_P4) << 35);
	var1 = ((var1 * var1 * (int64_t)bmp280_cal_data.dig_P3) >> 8) + ((var1 * (int64_t)bmp280_cal_data.dig_P2) << 12);
	var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bmp280_cal_data.dig_P1) >> 33;
	if(var1 == 0) return 0;
	p = 1048576 - adcP;
	p = (((p << 31) - var2) * 3125) / var1;
	var1 = (((int64_t)bmp280_cal_data.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
	var2 = (((int64_t)bmp280_cal_data.dig_P8) * p) >> 19;
	p = ((p + var1 + var2) >> 8) + (((int64_t)bmp280_cal_data.dig_P7) << 4);
	return (uint32_t)p;
}

void baro_poll(uint32_t diff_ms)
{
	tmr_poll += diff_ms;
	if(tmr_poll >= POLL_INTERVAL)
	{
		tmr_poll = 0;

		uint8_t data_tx[1 + BMP280_DATA_FRAME_SIZE] = {BMP280_PRESSURE_MSB_REG}, data_rx[1 + BMP280_DATA_FRAME_SIZE];

		CS_L;
		spi_trx(data_tx, data_rx, 1 + BMP280_DATA_FRAME_SIZE);
		CS_H;

		baro_data.pres_raw = (int32_t)((((uint32_t)(data_rx[1])) << 12) | (((uint32_t)(data_rx[2])) << 4) | ((uint32_t)data_rx[3] >> 4));
		baro_data.temp_raw = (int32_t)((((uint32_t)(data_rx[4])) << 12) | (((uint32_t)(data_rx[5])) << 4) | ((uint32_t)data_rx[6] >> 4));

		baro_data.temp = bmp280CompensateT(baro_data.temp_raw) / 10; // 0.1 * degrees
		baro_data.pres = bmp280CompensateP(baro_data.pres_raw) >> 8; // Pascal
		OD_RAM.x6101_baro.temp = baro_data.temp;
		OD_RAM.x6101_baro.pres = baro_data.pres;
	}
}