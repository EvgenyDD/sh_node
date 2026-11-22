#include "app_common.h"
#include "bus_t4.h"

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
	bus_t4_init();

	for(;;)
	{
		CAN_LOOP_PRE()
		{
			GENERIC_LED_FLASH();

			adc_track();
			if(aht21.exist) aht21_poll(diff_ms);
			bus_t4_poll(diff_ms);

			if(OD_RAM.x8200_ebus_cmd.enable)
			{
				//   SBS               55 0c 00 ff 00 66 01 05 9D 01 82 01 64 E6 0c
				//   STOP              55 0c 00 ff 00 66 01 05 9D 01 82 02 64 E5 0c
				//   OPEN              55 0c 00 ff 00 66 01 05 9D 01 82 03 00 80 0c
				//   CLOSE             55 0c 00 ff 00 66 01 05 9D 01 82 04 64 E3 0c
				//   PARENTAL OPEN 1   55 0c 00 ff 00 66 01 05 9D 01 82 05 64 E2 0c
				//   PARENTAL OPEN 2   55 0c 00 ff 00 66 01 05 9D 01 82 06 64 E1 0c
				uint8_t cmd_sts[] = {0x00, 0xff, 0x00, 0x66, 0x08, 0x06, 0x6B, 0x04, 0xD0, 0x99, 0x00, 0x00};

				uint8_t cmd_step[] = {0x00, 0xff, 0x00, 0x66, 0x01, 0x05, 0x9D, 0x01, 0x82, 0x01, 0x64, 0xE6}; // step-by-step, SBS
				uint8_t cmd_stop[] = {0x00, 0xff, 0x00, 0x66, 0x01, 0x05, 0x9D, 0x01, 0x82, 0x02, 0x64, 0xE5};
				uint8_t cmd_open[] = {0x00, 0xff, 0x00, 0x66, 0x01, 0x05, 0x9D, 0x01, 0x82, 0x03, 0x00, 0x80};
				uint8_t cmd_close[] = {0x00, 0xff, 0x00, 0x66, 0x01, 0x05, 0x9D, 0x01, 0x82, 0x04, 0x64, 0xE3};
				uint8_t cmd_parental_open1[] = {0x00, 0xff, 0x00, 0x66, 0x01, 0x05, 0x9D, 0x01, 0x82, 0x05, 0x64, 0xE2};
				uint8_t cmd_parental_open2[] = {0x00, 0xff, 0x00, 0x66, 0x01, 0x05, 0x9D, 0x01, 0x82, 0x06, 0x64, 0xE1};

				if(OD_RAM.x8200_ebus_cmd.enable == 1) bus_t4_send(cmd_sts, sizeof(cmd_sts));
				if(OD_RAM.x8200_ebus_cmd.enable == 2) bus_t4_send(cmd_step, sizeof(cmd_step));
				if(OD_RAM.x8200_ebus_cmd.enable == 3) bus_t4_send(cmd_stop, sizeof(cmd_stop));
				if(OD_RAM.x8200_ebus_cmd.enable == 4) bus_t4_send(cmd_open, sizeof(cmd_open));
				if(OD_RAM.x8200_ebus_cmd.enable == 5) bus_t4_send(cmd_close, sizeof(cmd_close));
				if(OD_RAM.x8200_ebus_cmd.enable == 6) bus_t4_send(cmd_parental_open1, sizeof(cmd_parental_open1));
				if(OD_RAM.x8200_ebus_cmd.enable == 7) bus_t4_send(cmd_parental_open2, sizeof(cmd_parental_open2));

				OD_RAM.x8200_ebus_cmd.enable = 0;
			}

			if(diff_ms)
			{
				OD_RAM.x6113_bus_t4_err_cnt[0] = bus.err_ovf;
				OD_RAM.x6113_bus_t4_err_cnt[1] = bus.err_crc;
				OD_RAM.x6113_bus_t4_err_cnt[2] = bus.err_pkt_short;
				OD_RAM.x6114_bus_t4_ok_cnt[0] = bus.ok_rx;
				OD_RAM.x6114_bus_t4_ok_cnt[1] = bus.ok_tx;
			}
		}
		CAN_LOOP_POST()
	}
}