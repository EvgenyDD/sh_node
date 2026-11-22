#include "app_common.h"

CONFIG_GENERIC_INIT();

#define MON_LOAD_THRS_US 1000
static uint32_t mon_load_sw_cnt[2] = {0}, mon_load_sw_time = 0;

void main(void)
{
	GENERIC_INIT_0();
	GENERIC_INIT();

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_1; // LED
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStruct);

	// GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2; // D0
	// GPIO_Init(GPIOB, &GPIO_InitStruct);

	// IN
	// GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	// GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8; // TRBS0
	// GPIO_Init(GPIOA, &GPIO_InitStruct);

	// GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10; // TRBS2
	// GPIO_Init(GPIOB, &GPIO_InitStruct);

	OD_PERSIST_COMM.x1016_consumerHeartbeatTime[0] = (1 /* master ID */ << 16) | 5000;

	spi_common_init();
	adc_init();
	aht21_init();

	for(;;)
	{
		CAN_LOOP_PRE()
		{
			GENERIC_LED_FLASH();

			if(adc_track())
			{
			}
			if(aht21.exist) aht21_poll(diff_ms);

			{
				mon_load_sw_time += diff_us;
				if(mon_load_sw_time >= 40000) // 2half-period
				{
					static uint8_t sw_state_prev = 0;
					// OD_RAM.x6107_sw_state.sw_state = ((mon_load_sw_cnt[0] > MON_LOAD_THRS_US) << 0) | // warm floor pump II
					// 								 ((mon_load_sw_cnt[1] > MON_LOAD_THRS_US) << 1);  // boiler switch
					if(sw_state_prev != OD_RAM.x6107_sw_state.sw_state)
					{
						sw_state_prev = OD_RAM.x6107_sw_state.sw_state;
						CO_TPDOsendRequest(&CO->TPDO[1]);
					}
					mon_load_sw_time = 0;
					memset(mon_load_sw_cnt, 0, sizeof(mon_load_sw_cnt));
				}
				// if(!(PIN_GET_(GPIOA, 8))) mon_load_sw_cnt[0] += diff_us;  // warm floor pump II
				// if(!(PIN_GET_(GPIOB, 10))) mon_load_sw_cnt[1] += diff_us; // boiler switch
			}

			// PIN_WR_(GPIOB, 2, OD_RAM.x7000_sw_ctrl.sw_ctrl & (1 << 0)); // warm floor pump II
		}
		CAN_LOOP_POST()
	}
}