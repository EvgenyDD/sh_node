#include "app_common.h"

config_entry_t g_device_config[] = {
	{"can_id", sizeof(pending_can_node_id), 0, &pending_can_node_id},
	{"can_baud", sizeof(pending_can_baud), 0, &pending_can_baud},
	{"hb_prod_ms", sizeof(OD_PERSIST_COMM.x1017_producerHeartbeatTime), 0, &OD_PERSIST_COMM.x1017_producerHeartbeatTime},
	{"ds_addr", sizeof(OD_RAM.x8102_ds18b20_cfg), 0, OD_RAM.x8102_ds18b20_cfg},
	{"ds_cnt", sizeof(OD_RAM.x8101_ds18b20_cmd.num_sensors), 0, &OD_RAM.x8101_ds18b20_cmd.num_sensors},
};
const uint32_t g_device_config_count = sizeof(g_device_config) / sizeof(g_device_config[0]);

#define MON_LOAD_THRS_US 1000
static uint32_t mon_load_sw_cnt[5] = {0}, mon_load_sw_time = 0;

void main(void)
{
	GENERIC_INIT_0();
	GENERIC_INIT();

	// OUT
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10; // CS0
	GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_13 | GPIO_Pin_14; // D0, D2, D3
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_15; // D1
	GPIO_Init(GPIOC, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_1; // LED
	GPIO_Init(GPIOD, &GPIO_InitStruct);

	// IN
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_15; // TRBS2, TRBS3, PIR, PPM
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0; // MQ_EN
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

			{
				mon_load_sw_time += diff_us;
				if(mon_load_sw_time >= 40000) // 2half-period
				{
					OD_RAM.x6107_sw_state.sw_state = ((mon_load_sw_cnt[0] > MON_LOAD_THRS_US) << 0) | // radiators
													 ((mon_load_sw_cnt[1] > MON_LOAD_THRS_US) << 1) | // warm floor I
													 ((mon_load_sw_cnt[2] > MON_LOAD_THRS_US) << 2) | // pool
													 ((mon_load_sw_cnt[3] > MON_LOAD_THRS_US) << 3) | // heat circulation
													 ((mon_load_sw_cnt[4] > MON_LOAD_THRS_US) << 4);  // solar?

					mon_load_sw_time = 0;
					memset(mon_load_sw_cnt, 0, sizeof(mon_load_sw_cnt));
				}
				if(!(PIN_GET_(GPIOD, 0))) mon_load_sw_cnt[0] += diff_us;  // radiators
				if(!(PIN_GET_(GPIOB, 15))) mon_load_sw_cnt[1] += diff_us; // warm floor I
				if(!(PIN_GET_(GPIOB, 10))) mon_load_sw_cnt[2] += diff_us; // pool
				if(!(PIN_GET_(GPIOB, 11))) mon_load_sw_cnt[3] += diff_us; // heat circulation
				if(!(PIN_GET_(GPIOB, 12))) mon_load_sw_cnt[4] += diff_us; // solar?
			}

			PIN_WR_(GPIOB, 2, OD_RAM.x7000_sw_ctrl.sw_ctrl & (1 << 0));	 // radiators
			PIN_WR_(GPIOC, 15, OD_RAM.x7000_sw_ctrl.sw_ctrl & (1 << 1)); // warm floor I
			PIN_WR_(GPIOB, 13, OD_RAM.x7000_sw_ctrl.sw_ctrl & (1 << 2)); // pool
			PIN_WR_(GPIOB, 14, OD_RAM.x7000_sw_ctrl.sw_ctrl & (1 << 2)); // heat circulation
			PIN_WR_(GPIOA, 10, OD_RAM.x7000_sw_ctrl.sw_ctrl & (1 << 2)); // solar?
		}
		CAN_LOOP_POST()
	}
}