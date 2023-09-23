#include "CANopen.h"
#include "OD.h"
#include "adc.h"
#include "apps.h"
#include "debounce.h"
#include "gps.h"
#include "mag.h"
#include "platform.h"

debounce_ctrl_t deb_wind = {0}, deb_rain = {0};

void meteo_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	debounce_init(&deb_wind, 4);
	debounce_init(&deb_rain, 4);
	gps_init();
}

void meteo_poll(uint32_t diff_ms)
{
	debounce_cb(&deb_wind, GPIOB->IDR & (1 << 10), diff_ms);
	debounce_cb(&deb_rain, GPIOB->IDR & (1 << 11), diff_ms);
	gps_poll();
	if(mag_data.sensor_present) mag_poll(diff_ms);

	OD_RAM.x6102_meteo.wind_acc += deb_wind.pressed_shot ? 1 : 0;
	OD_RAM.x6102_meteo.rain_acc += deb_rain.pressed_shot ? 1 : 0;
	OD_RAM.x6102_meteo.wind_heading = OD_RAM.x6000_adc.ai0;
	OD_RAM.x6102_meteo.rain_temp = OD_RAM.x6000_adc.ai1;
	OD_RAM.x6102_meteo.solar = OD_RAM.x6000_adc.ai2;
}
