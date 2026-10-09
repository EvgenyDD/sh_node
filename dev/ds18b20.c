#include "ds18b20.h"
#include "CANopen.h"
#include "OD.h"
#include "cfg_device.h"
#include "platform.h"
#include "stm32f10x.h"

#ifdef CFG_USE_DS18B20

#define MAX_SENSORS 16
#define CONV_TIME_MS 750

static uint32_t readout_idx = 0;

static uint8_t one_wire_crc8(const uint8_t *data, uint8_t len)
{
	uint8_t crc = 0;
	for(uint8_t i = 0; i < len; i++)
	{
		uint8_t inbyte = data[i];
		for(uint8_t j = 0; j < 8; j++)
		{
			uint8_t mix = (crc ^ inbyte) & 0x01;
			crc >>= 1;
			if(mix)
			{
				crc ^= 0x8C; // Полином Dallas 1-Wire
			}
			inbyte >>= 1;
		}
	}
	return crc;
}

static void uart_set_baud(uint32_t new_baudrate)
{
	USART_Cmd(USART1, DISABLE);

	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = new_baudrate;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

	USART_Init(USART1, &USART_InitStructure);
	USART_Cmd(USART1, ENABLE);
}

static uint8_t uart_trx(uint8_t value)
{
	volatile uint32_t status = USART1->SR;
	volatile uint32_t dummy = USART1->DR;
	(void)status;
	(void)dummy;

	uint32_t timeout = 20000;
	while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
	{
		if(--timeout == 0) return 0xFF;
	}

	USART_SendData(USART1, value);

	timeout = 20000;
	while(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET)
	{
		if(--timeout == 0) return 0xFF; // disconnected/noisy -> exit, 1-Wire "line idle (logic high)
	}

	return USART_ReceiveData(USART1);
}

static uint8_t one_wire_reset(void)
{
	uart_set_baud(9600);

	uint8_t presence_byte = uart_trx(0xF0); // if sensor is present on the bus - pull down, byte received != 0xF0.

	uart_set_baud(115200);

	return presence_byte != 0xF0 ? 1 : 0; // 0 if nobody
}

static void one_wire_wr_bit(uint8_t bit)
{
	uart_trx(bit ? 0xFF : 0);
}

static uint8_t one_wire_rd_bit(void)
{
	return uart_trx(0xFF) == 0xFF ? 1 : 0; // release the line (send 0xFF). Sensor wants to signal a "0": pull the line low, return != 0xFF
}

static void one_wire_wr_byte(uint8_t byte)
{
	for(uint8_t i = 0; i < 8; i++)
	{
		one_wire_wr_bit(byte & (1 << i));
	}
}

static uint8_t one_wire_rd_byte(void)
{
	uint8_t byte = 0;
	for(uint8_t i = 0; i < 8; i++)
	{
		if(one_wire_rd_bit()) byte |= (1 << i);
	}
	return byte;
}

// static int read_temp_single(int16_t *temp)
// {
// 	if(!one_wire_reset()) return -1;

// 	one_wire_wr_byte(0xCC); // Skip ROM
// 	one_wire_wr_byte(0xBE); // Read Scratchpad

// 	uint8_t temp_lsb = one_wire_rd_byte();
// 	uint8_t temp_msb = one_wire_rd_byte();
// 	*temp = (temp_msb << 8) | temp_lsb; // float temperature = (float)raw_temp / 16.0;
// 	return 0;
// }

static int read_temp_addr(const uint8_t *rom, int16_t *temp)
{
	if(!one_wire_reset()) return -1;

	one_wire_wr_byte(0x55); // Match ROM
	for(uint8_t i = 0; i < 8; i++)
	{
		one_wire_wr_byte(rom[i]);
	}

	one_wire_wr_byte(0xBE); // Read Scratchpad

	uint8_t scratchpad[9];
	for(uint8_t i = 0; i < 9; i++)
	{
		scratchpad[i] = one_wire_rd_byte();
	}

	if(one_wire_crc8(scratchpad, 9) != 0) return -2;

	*temp = (scratchpad[1] << 8) | scratchpad[0];
	return 0;
}

void ds18b20_init(uint32_t baudrate)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = baudrate; // 9600 reset, 115200 data
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART1, &USART_InitStructure);

	USART_HalfDuplexCmd(USART1, ENABLE);
	USART_Cmd(USART1, ENABLE);
}

void ds18b20_poll(uint32_t diff_ms)
{
	if(readout_idx < OD_RAM.x8101_ds18b20_cmd.num_sensors)
	{
		int16_t temp;
		if(read_temp_addr(OD_RAM.x8102_ds18b20_cfg[readout_idx], &temp) == 0)
		{
			OD_RAM.x6102_ds18b20[readout_idx] = temp;
		}
		else
		{
			OD_RAM.x6102_ds18b20[readout_idx] = INT16_MIN;
		}

		readout_idx++;
		return;
	}

	static uint32_t tmr = 0;
	if(tmr)
	{
		tmr = tmr > diff_ms ? tmr - diff_ms : 0;
		if(tmr == 0) readout_idx = 0;
		return;
	}

	if(OD_RAM.x8101_ds18b20_cmd.detect != 0)
	{
		OD_RAM.x8101_ds18b20_cmd.num_sensors = ds18b20_detect();
		OD_RAM.x8101_ds18b20_cmd.detect = 0;
		return;
	}

	{								  // start conversion ALL sensors
		if(!one_wire_reset()) return; // no response

		one_wire_wr_byte(0xCC); // Skip ROM
		one_wire_wr_byte(0x44); // Start Conversion

		tmr = CONV_TIME_MS;
	}

	return;
}

uint8_t ds18b20_detect(void)
{
	uint8_t last_mismatch = 0;
	uint8_t last_device_flag = 0;
	uint8_t current_uid[8] = {0};

	uint8_t uid_count = 0;

	do
	{
		if(!one_wire_reset()) break; // check any sensor with Reset Pulse

		one_wire_wr_byte(0xF0); // Search ROM

		uint8_t id_bit_number = 1;
		uint8_t last_zero = 0;
		uint8_t rom_byte_number = 0;
		uint8_t rom_byte_mask = 1;
		uint8_t search_direction = 0;
		uint8_t search_result = 1;

		while(id_bit_number <= 64) // 64bit  UID
		{
			uint8_t id_bit = one_wire_rd_bit();
			uint8_t cmp_id_bit = one_wire_rd_bit();

			if((id_bit == 1) && (cmp_id_bit == 1)) // If both bits are 1, a bus error has occurred or the sensors have disconnected
			{
				search_result = 0;
				break;
			}
			else
			{

				if(id_bit != cmp_id_bit) // bits are different -> then all sensors on the bus have the same bit at this position
				{
					search_direction = id_bit; // select this bit as the direction of movement
				}
				else
				{
					if(id_bit_number < last_mismatch) // conflict: there are sensors with both "0" and "1" at the current bit position
					{
						search_direction = ((current_uid[rom_byte_number] & rom_byte_mask) > 0);
					}
					else
					{
						search_direction = (id_bit_number == last_mismatch); // exactly on it, we select "1"; if we are to the right, we select "0"
					}

					if(search_direction == 0) last_zero = id_bit_number; // chose "0", we mark this fork as the very last one
				}

				if(search_direction == 1) // write the selected bit to the UID currently being assembled
				{
					current_uid[rom_byte_number] |= rom_byte_mask;
				}
				else
				{
					current_uid[rom_byte_number] &= ~rom_byte_mask;
				}

				one_wire_wr_bit(search_direction); // send selected direction to the sensors to filter out the unwanted ones

				id_bit_number++;
				rom_byte_mask <<= 1;
				if(rom_byte_mask == 0)
				{
					rom_byte_number++;
					rom_byte_mask = 1;
				}
			}
		}

		if(search_result) // 64-bit pass completed successfully
		{
			if(uid_count >= MAX_SENSORS) return uid_count;

			if(one_wire_crc8(current_uid, 8) == 0)
			{
				for(uint8_t i = 0; i < 8; i++)
				{
					OD_RAM.x8102_ds18b20_cfg[uid_count][i] = current_uid[i];
				}
				uid_count++;
			}
			last_mismatch = last_zero;

			if(last_mismatch == 0) last_device_flag = 1;
		}
		else
		{
			break; // hardware failure during bit reading - abort the search
		}

	} while(!last_device_flag);

	return uid_count;
}

#endif