#include "app_common.h"
#include "ebus.h"

volatile uint8_t trig = 0;

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

	OD_PERSIST_COMM.x1016_consumerHeartbeatTime[0] = (1 /* master ID */ << 16) | 5000;

	spi_common_init();
	adc_init();
	aht21_init();
	ebus_init();
	pir_init();

	for(;;)
	{
		CAN_LOOP_PRE()
		{
			// GENERIC_LED_FLASH();
			static uint32_t led_tim = 0;
			led_tim += diff_ms;
			if(led_tim >= 500) led_tim = 0;
			PIN_WR_(GPIOD, 1, led_tim < (OD_RAM.x6104_pir.pir_state ? 500 : 5));

			adc_track();
			if(aht21.exist) aht21_poll(diff_ms);
			ebus_poll(diff_ms);
			pir_poll(diff_ms);

			static uint32_t f = 0;
			f += diff_ms;
			if(f >= 500)
			{
				f = 0;
				trig = 1;
			}
		}
		CAN_LOOP_POST()
	}
}