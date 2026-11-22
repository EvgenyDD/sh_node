#include "app_common.h"
#include <stdio.h>

CONFIG_GENERIC_INIT();

void main(void)
{
	GENERIC_INIT_0();
	GENERIC_INIT();

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_1; // LED
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12; // PIR
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10; // TRBS2
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	OD_PERSIST_COMM.x1016_consumerHeartbeatTime[0] = (1 /* master ID */ << 16) | 5000;

	spi_common_init();
	baro_init();
	adc_init();
	aht21_init();
	ens160_init();
	tm1637_init();
	ds18b20_init(9600);

	tm1637_brightness(7);
	tm1637_str("1234");

	for(;;)
	{
		CAN_LOOP_PRE()
		{
			GENERIC_LED_FLASH();

			if(adc_track())
			{
			}

			if(aht21.exist) aht21_poll(diff_ms);
			if(ens160.exist) ens160_poll(diff_ms);
			ds18b20_poll(diff_ms);

			static uint32_t c = 0;
			c += diff_ms;
			if(c >= 100000) c = 0;

			static uint32_t p = 0;
			p += diff_ms;

			static uint32_t upd_c = 0;
			upd_c += diff_ms;
			if(upd_c >= 40)
			{
				static uint32_t b = 0;
				b++;

				upd_c = 0;
				char txt[5] = {0};
				tm1637_brightness((b % 120) / 15);

				sprintf(txt, "%04ld", c / 10);
				if(p >= 4000) p = 0;
				txt[p / 1000] = '.';
				tm1637_str(txt);
			}
		}
		CAN_LOOP_POST()
	}
}