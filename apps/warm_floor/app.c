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

	adc_init();
	aht21_init();
	ds18b20_init(9600);

	for(;;)
	{
		CAN_LOOP_PRE()
		{
			GENERIC_LED_FLASH();

			if(adc_track())
			{
				// OD_RAM.x6000_adc.ai0 = (int16_t)adc_val.sns_ai[0];
				// OD_RAM.x6000_adc.ai1 = (int16_t)adc_val.sns_ai[1];
				// OD_RAM.x6000_adc.ai2 = (int16_t)adc_val.sns_ai[2];
				// OD_RAM.x6000_adc.ai3 = (int16_t)adc_val.sns_ai[3];
				// OD_RAM.x6000_adc.aux = (int16_t)adc_val.sns_mq2;
				// OD_RAM.x6000_adc.srv = (int16_t)adc_val.srv_pos;
				// OD_RAM.x6000_adc.vin = (int16_t)adc_val.vin;
				// OD_RAM.x6000_adc.i0 = (int16_t)adc_val.sns_i[0];
				// OD_RAM.x6000_adc.i1 = (int16_t)adc_val.sns_i[1];
				// OD_RAM.x6000_adc.t_mcu = (int16_t)adc_val.t_mcu;
			}
			if(aht21.exist) aht21_poll(diff_ms);
			ds18b20_poll(diff_ms);
		}
		CAN_LOOP_POST()
	}
}