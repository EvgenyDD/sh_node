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
			pir_poll(diff_ms, 0);

			if(diff_ms)
			{
				static uint8_t ebus_prev_en = 0;
				if(ebus_prev_en != OD_RAM.x8200_ebus_cmd.enable)
				{
					ebus_prev_en = OD_RAM.x8200_ebus_cmd.enable;
					ebus_enable(OD_RAM.x8200_ebus_cmd.enable);
				}

				for(uint32_t i = 0; i < PH_CNT; i++)
				{
					OD_RAM.x6111_ebus_err_ph_cnt[i] = boiler.err[i];
				}

				OD_RAM.x6113_ebus_err_cnt[0] = boiler.err_ovf;
				OD_RAM.x6113_ebus_err_cnt[1] = boiler.err_nak;
				OD_RAM.x6113_ebus_err_cnt[2] = boiler.err_unknw_resp;
				OD_RAM.x6113_ebus_err_cnt[3] = boiler.err_big_sz;
				OD_RAM.x6113_ebus_err_cnt[4] = boiler.err_crc;
				OD_RAM.x6113_ebus_err_cnt[5] = boiler.err_sent_setpoint;

				OD_RAM.x6114_ebus_ok_cnt[0] = boiler.ok_ack;
				OD_RAM.x6114_ebus_ok_cnt[1] = boiler.ok_pkt_rx;
				OD_RAM.x6114_ebus_ok_cnt[2] = boiler.ok_cmd_pkt_rx;
			}
		}
		CAN_LOOP_POST()
	}
}