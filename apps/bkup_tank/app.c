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

	// PPM (B15) - particle sensor, AIN0 - level

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1; // 0 - MQ2_EN, 1 - LED
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_14; // 14 - SEN (particle sensor)
	GPIO_Init(GPIOC, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_6 | GPIO_Pin_7; // FAN, VALVE, PUMP
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GENERIC_INIT();

	OD_PERSIST_COMM.x1016_consumerHeartbeatTime[0] = (1 /* master ID */ << 16) | 5000;

	adc_init();
	aht21_init();
	dsm501_init();
	ds18b20_init(9600);

	for(;;)
	{
		CAN_LOOP_PRE()
		{
			GENERIC_LED_FLASH();

			if(adc_track())
			{
				OD_RAM.x6100_adc.tank_level = adc_val.sns_ai[0];
			}
			if(aht21.exist) aht21_poll(diff_ms);

			dsm501_poll(diff_ms);
			ds18b20_poll(diff_ms);

			PIN_WR_(GPIOB, 4, OD_RAM.x7000_sw_ctrl.sw_ctrl & (1 << 0)); // fan
			// PIN_WR_(GPIOB, 7, OD_RAM.x7000_sw_ctrl.sw_ctrl & (1 << 1)); // pump
			// PIN_WR_(GPIOB, 6, OD_RAM.x7000_sw_ctrl.sw_ctrl & (1 << 2)); // valve
		}
		CAN_LOOP_POST()
	}
}