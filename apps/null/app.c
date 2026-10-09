#include "app_common.h"

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
	ds18b20_init(9600);

	static uint32_t c = 0;

	for(;;)
	{
		CAN_LOOP_PRE()
		{
			// GENERIC_LED_FLASH();
			static uint32_t led_tim = 0;
			led_tim += diff_ms;
			if(led_tim >= 500) led_tim = 0;
			// PIN_WR_(GPIOD, 1, led_tim < (PIN_GET_(GPIOB, 12) ? 400 : 5));
			// PIN_WR_(GPIOD, 1, led_tim < (c > 5 ? 400 : 5));

			if(!PIN_GET_(GPIOB, 10)) c++;

			if(adc_track())
			{
			}
			if(aht21.exist) aht21_poll(diff_ms);
			ds18b20_poll(diff_ms);
		}
		CAN_LOOP_POST()
	}
}