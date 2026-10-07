#include "app_common.h"
#include "gps.h"
#include "mag.h"

CONFIG_GENERIC_INIT();

#define V0e 840
#define V1se 1485
#define V2s 2048
#define V3ne 2775
#define V4sw 3292
#define V5n 3664
#define V6nw 3862
#define V7w 3966

enum
{
	HEAD_E = 0,
	HEAD_NE,
	HEAD_N,
	HEAD_NW,
	HEAD_W,
	HEAD_SW,
	HEAD_S,
	HEAD_SE,
};

debounce_ctrl_t deb_wind = {0}, deb_rain = {0};

// static const uint16_t head_values[] = {840, 1485, 2048, 2775, 3292, 3664, 3862, 3966};

void main(void)
{
	GENERIC_INIT_0();

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1; // 0 - MQ2_EN, 1 - LED
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15; // 14 - SEN, 15 - TP
	GPIO_Init(GPIOC, &GPIO_InitStruct);

	GENERIC_INIT();

	OD_PERSIST_COMM.x1016_consumerHeartbeatTime[0] = (1 /* master ID */ << 16) | 5000;

	spi_common_init();

	baro_init();
	adc_init();
	aht21_init();

	// GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11; // 10 - wind tach, 11 - rain tach
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	debounce_init(&deb_wind, 1);
	debounce_init(&deb_rain, 1);

	gps_init();
	mag_init();

	for(;;)
	{
		CAN_LOOP_PRE()
		{
			GENERIC_LED_FLASH();

			if(adc_track())
			{
				OD_RAM.x6000_adc.ai0 = (int16_t)adc_val.sns_ai[0];
				OD_RAM.x6000_adc.ai1 = (int16_t)adc_val.sns_ai[1];
				OD_RAM.x6000_adc.ai2 = (int16_t)adc_val.sns_ai[2];
				OD_RAM.x6000_adc.ai3 = (int16_t)adc_val.sns_ai[3];
				OD_RAM.x6000_adc.aux = (int16_t)adc_val.sns_mq2;
				OD_RAM.x6000_adc.srv = (int16_t)adc_val.srv_pos;
				OD_RAM.x6000_adc.vin = (int16_t)adc_val.vin;
				OD_RAM.x6000_adc.i0 = (int16_t)adc_val.sns_i[0];
				OD_RAM.x6000_adc.i1 = (int16_t)adc_val.sns_i[1];
				OD_RAM.x6000_adc.t_mcu = (int16_t)adc_val.t_mcu;

				OD_RAM.x6102_meteo.rain_temp = (int16_t)ntc10k_adc_to_degc(adc_val.sns_ai[1]);
			}

			if(baro.exist) baro_poll(diff_ms);

			debounce_cb(&deb_wind, !(GPIOB->IDR & (1 << 10)), diff_ms);
			debounce_cb(&deb_rain, !(GPIOB->IDR & (1 << 11)), diff_ms);

			gps_poll();

			if(mag_data.exist)
			{
				mag_poll(diff_ms);
				OD_RAM.x6104_mag.field_x = mag_data.mag_field[0];
				OD_RAM.x6104_mag.field_y = mag_data.mag_field[1];
				OD_RAM.x6104_mag.field_z = mag_data.mag_field[2];
			}

			OD_RAM.x6102_meteo.wind_acc += deb_wind.pressed_shot ? 1 : 0;
			OD_RAM.x6102_meteo.rain_acc += deb_rain.pressed_shot ? 1 : 0;
			OD_RAM.x6102_meteo.solar = (int16_t)adc_val.sns_ai[0];

			// determine heading
			const int16_t v = (int16_t)adc_val.sns_ai[2];
			uint8_t head;
			if(v <= (V0e + V1se) / 2)
			{
				head = HEAD_E;
				OD_RAM.x6102_meteo.wind_acc_e += deb_wind.pressed_shot ? 1 : 0;
			}
			else if(v <= (V1se + V2s) / 2)
			{
				head = HEAD_SE;
				OD_RAM.x6102_meteo.wind_acc_se += deb_wind.pressed_shot ? 1 : 0;
			}
			else if(v <= (V2s + V3ne) / 2)
			{
				head = HEAD_S;
				OD_RAM.x6102_meteo.wind_acc_s += deb_wind.pressed_shot ? 1 : 0;
			}
			else if(v <= (V3ne + V4sw) / 2)
			{
				head = HEAD_NE;
				OD_RAM.x6102_meteo.wind_acc_ne += deb_wind.pressed_shot ? 1 : 0;
			}
			else if(v <= (V4sw + V5n) / 2)
			{
				head = HEAD_SW;
				OD_RAM.x6102_meteo.wind_acc_sw += deb_wind.pressed_shot ? 1 : 0;
			}
			else if(v <= (V5n + V6nw) / 2)
			{
				head = HEAD_N;
				OD_RAM.x6102_meteo.wind_acc_n += deb_wind.pressed_shot ? 1 : 0;
			}
			else if(v <= (V6nw + V7w) / 2)
			{
				head = HEAD_NW;
				OD_RAM.x6102_meteo.wind_acc_nw += deb_wind.pressed_shot ? 1 : 0;
			}
			else
			{
				head = HEAD_W;
				OD_RAM.x6102_meteo.wind_acc_w += deb_wind.pressed_shot ? 1 : 0;
			}
			OD_RAM.x6102_meteo.wind_heading = head;
		}
		CAN_LOOP_POST()
	}
}
