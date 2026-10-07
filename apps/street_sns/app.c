#include "app_common.h"
#include "ds18b20.h"

CONFIG_GENERIC_INIT();

#define PIR_OFF_TO_MS 20000

static uint32_t pir_off_tmr = 0;

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

	for(;;)
	{
		CAN_LOOP_PRE()
		{
			// GENERIC_LED_FLASH();
			static uint32_t led_tim = 0;
			led_tim += diff_ms;
			if(led_tim >= 500) led_tim = 0;
			PIN_WR_(GPIOD, 1, led_tim < (OD_RAM.x6104_pir.pir_state ? 500 : 5));

			static uint32_t h = 0, blo = 0;
			h += diff_ms;
			if(h >= 600)
			{
				h = 0;
				blo++;
				if(blo == 5)
				{
					blo = 0;
					ds18b20_detect();
					// GPIOD->ODR ^= 1 << 1;
				}
			}

			adc_track();
			if(aht21.exist) aht21_poll(diff_ms);
			if(baro.exist) baro_poll(diff_ms);
			ds18b20_read(diff_ms);
			OD_RAM.x6102_ds18b20[0] = ds18b20_get_temp()[0];

			{
				static bool pir_prev = 0;
				bool pir_now = PIN_GET_(GPIOB, 12);
				if(pir_now && !pir_prev)
				{
					pir_off_tmr = PIR_OFF_TO_MS;
					OD_RAM.x6104_pir.pir_state = 1;
					CO_TPDOsendRequest(&CO->TPDO[1]);
				}
				if(pir_off_tmr)
				{
					pir_off_tmr = pir_off_tmr > diff_ms ? pir_off_tmr - diff_ms : 0;
					if(pir_off_tmr == 0)
					{
						OD_RAM.x6104_pir.pir_state = 0;
						CO_TPDOsendRequest(&CO->TPDO[1]);
					}
				}
				pir_prev = pir_now;
			}
		}
		CAN_LOOP_POST()
	}
}