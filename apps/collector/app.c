#include "app_common.h"

config_entry_t g_device_config[] = {
	{"can_id", sizeof(pending_can_node_id), 0, &pending_can_node_id},
	{"can_baud", sizeof(pending_can_baud), 0, &pending_can_baud},
	{"hb_prod_ms", sizeof(OD_PERSIST_COMM.x1017_producerHeartbeatTime), 0, &OD_PERSIST_COMM.x1017_producerHeartbeatTime},
	{"ds_addr", sizeof(OD_RAM.x8102_ds18b20_cfg), 0, OD_RAM.x8102_ds18b20_cfg},
	{"ds_cnt", sizeof(OD_RAM.x8101_ds18b20_cmd.num_sensors), 0, &OD_RAM.x8101_ds18b20_cmd.num_sensors},
};
const uint32_t g_device_config_count = sizeof(g_device_config) / sizeof(g_device_config[0]);

void main(void)
{
	GENERIC_INIT_0();
	GENERIC_INIT();

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_1; // LED
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_13 | GPIO_Pin_14; // D0, D2, D3
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_15; // D1
	GPIO_Init(GPIOC, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_15; // TRBS2, TRBS3, PPM
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0; // MQ_EN
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOD, &GPIO_InitStruct);

	OD_PERSIST_COMM.x1016_consumerHeartbeatTime[0] = (1 /* master ID */ << 16) | 5000;

	spi_common_init();
	adc_init();
	aht21_init();
	ds18b20_init(9600);
	tacho_init();

	static uint32_t c = 0;

	for(;;)
	{
		CAN_LOOP_PRE()
		{
			GENERIC_LED_FLASH();

			if(!PIN_GET_(GPIOB, 10)) c++;

			if(adc_track())
			{
				OD_RAM.x6100_adc.pres = adc_val.sns_ai[3];
			}
			if(aht21.exist) aht21_poll(diff_ms);
			ds18b20_poll(diff_ms);
			tacho_poll(diff_ms);

			PIN_WR_(GPIOB, 2, OD_RAM.x7000_sw_ctrl.sw_ctrl & (1 << 0));	 // radiators
			PIN_WR_(GPIOC, 15, OD_RAM.x7000_sw_ctrl.sw_ctrl & (1 << 1)); // warm floor I
			PIN_WR_(GPIOB, 13, OD_RAM.x7000_sw_ctrl.sw_ctrl & (1 << 2)); // pool
			PIN_WR_(GPIOB, 14, OD_RAM.x7000_sw_ctrl.sw_ctrl & (1 << 3)); // heat circulation

			OD_RAM.x6107_sw_state.sw_state = (PIN_GET_(GPIOD, 0) << 0) |  // radiators
											 (PIN_GET_(GPIOB, 15) << 1) | // warm floor I
											 (PIN_GET_(GPIOB, 10) << 2) | // pool
											 (PIN_GET_(GPIOB, 11) << 3);  // heat circulation
		}
		CAN_LOOP_POST()
	}
}