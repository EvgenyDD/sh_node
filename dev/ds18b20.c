#include "ds18b20.h"
#include "platform.h"
#include "stm32f10x.h"

#define MAX_SENSORS 16

#define CONV_TIME_MS 750

struct
{
	int16_t rawtemp;
	uint32_t tmr;
	uint8_t temp[2];
	uint32_t c;

	uint32_t readout_index;

	int16_t sensor_temperatures[MAX_SENSORS];
} ds18b20 = {0};

uint8_t uid_table[MAX_SENSORS][8] = {0};
uint32_t uid_count = 0;

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
	// В STM32 F1 чтение регистра SR, а затем регистра DR автоматически сбрасывает ORE, FE и NE
	volatile uint32_t status = USART1->SR;
	volatile uint32_t dummy = USART1->DR;
	(void)status;
	(void)dummy;

	uint32_t timeout = 20000; // Подберите экспериментально (обычно хватает нескольких тысяч циклов)
	while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
	{
		if(--timeout == 0)
		{
			return 0xFF; // Если зависли на передаче — выходим с флагом "пустой шины"
		}
	}

	USART_SendData(USART1, value);

	timeout = 20000;
	while(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET)
	{
		if(--timeout == 0)
		{
			// Если провод отключен или зашумлен, мы не зависнем, а просто выйдем.
			// Возвращаем 0xFF, что для 1-Wire означает "линия пустая (логическая единица)"
			return 0xFF;
		}
	}

	return USART_ReceiveData(USART1);
}

static uint8_t one_wire_reset(void)
{
	uart_set_baud(9600);

	// Шлем 0xF0. Если датчик на шине есть, он прижмет линию к нулю, и прилетевший обратно байт НЕ будет равен 0xF0.
	uint8_t presence_byte = uart_trx(0xF0);

	uart_set_baud(115200);

	return presence_byte != 0xF0 ? 1 : 0; // 0 if nobody
}

static void one_wire_wr_bit(uint8_t bit)
{
	uart_trx(bit ? 0xFF : 0);
}

static uint8_t one_wire_rd_bit(void)
{
	return uart_trx(0xFF) == 0xFF ? 1 : 0; // Чтобы прочитать бит, мы отпускаем линию (шлем 0xFF) Если датчик хочет сказать "0", он прижмет линию, и вернется НЕ 0xFF
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

void ds18b20_detect(void)
{
	uint8_t last_mismatch = 0;
	uint8_t last_device_flag = 0;
	uint8_t current_uid[8] = {0};

	uid_count = 0;

	do
	{
		if(!one_wire_reset()) break; // Проверяем наличие датчиков на шине через Reset Pulse

		one_wire_wr_byte(0xF0); // Search ROM

		uint8_t id_bit_number = 1;
		uint8_t last_zero = 0;
		uint8_t rom_byte_number = 0;
		uint8_t rom_byte_mask = 1;
		uint8_t search_direction = 0;
		uint8_t search_result = 1; // Флаг успешного прохода по дереву адресов

		while(id_bit_number <= 64) // Проход по всем 64 битам уникального ID датчика
		{
			// Читаем прямой бит и его инверсную копию
			uint8_t id_bit = one_wire_rd_bit();
			uint8_t cmp_id_bit = one_wire_rd_bit();

			// Если оба бита равны 1 — на шине произошла ошибка или датчики отключились
			if((id_bit == 1) && (cmp_id_bit == 1))
			{
				search_result = 0;
				break;
			}
			else
			{
				// Если биты разные, то у всех датчиков на шине в этой позиции одинаковый бит
				if(id_bit != cmp_id_bit)
				{
					search_direction = id_bit; // Выбираем этот бит как направление движения
				}
				else
				{
					// Коллизия: есть датчики как с "0", так и с "1" в текущей позиции бита
					if(id_bit_number < last_mismatch)
					{
						// Если мы левее прошлой развилки, идем по сохраненному пути
						search_direction = ((current_uid[rom_byte_number] & rom_byte_mask) > 0);
					}
					else
					{
						// Если мы на уровне или правее прошлой развилки:
						// Если мы точно на ней — выбираем "1", если правее — выбираем "0"
						search_direction = (id_bit_number == last_mismatch);
					}

					if(search_direction == 0) last_zero = id_bit_number; // Если выбрали "0", запоминаем эту развилку как самую последнюю
				}

				if(search_direction == 1) // Записываем выбранный бит в текущий собираемый UID
				{
					current_uid[rom_byte_number] |= rom_byte_mask;
				}
				else
				{
					current_uid[rom_byte_number] &= ~rom_byte_mask;
				}

				one_wire_wr_bit(search_direction); // Отправляем выбранное направление датчикам, чтобы отсечь ненужные

				id_bit_number++;
				rom_byte_mask <<= 1;
				if(rom_byte_mask == 0)
				{
					rom_byte_number++;
					rom_byte_mask = 1;
				}
			}
		}

		if(search_result) // Если проход по 64 битам завершился успешно
		{
			if(uid_count >= MAX_SENSORS) return;

			if(one_wire_crc8(current_uid, 8) == 0)
			{
				for(uint8_t i = 0; i < 8; i++)
				{
					uid_table[uid_count][i] = current_uid[i];
				}
				uid_count++; // Увеличиваем счетчик найденных датчиков
			}
			last_mismatch = last_zero;

			if(last_mismatch == 0) last_device_flag = 1; // Если развилок больше не осталось, значит мы нашли все датчики
		}
		else
		{

			break; // В случае аппаратного сбоя при чтении битов прерываем поиск
		}

	} while(!last_device_flag);
}

// static int read_temp_single(int16_t *temp)
// {
// 	if(!one_wire_reset()) return -1;

// 	one_wire_wr_byte(0xCC); // Skip ROM
// 	one_wire_wr_byte(0xBE); // Read Scratchpad (чтение памяти датчика)

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

int ds18b20_read(uint32_t diff_ms)
{
	if(ds18b20.readout_index < uid_count)
	{
		int16_t temp;
		if(read_temp_addr(uid_table[ds18b20.readout_index], &temp) == 0)
		{
			ds18b20.sensor_temperatures[ds18b20.readout_index] = temp;
		}
		else
		{
			ds18b20.sensor_temperatures[ds18b20.readout_index] = -100;
		}

		ds18b20.readout_index++;
	}

	if(ds18b20.tmr)
	{
		if(ds18b20.tmr > diff_ms)
		{
			ds18b20.tmr -= diff_ms;
			return 2;
		}
		else
		{
			ds18b20.tmr = 0;
			ds18b20.readout_index = 0;
		}
		return 0;
	}
	if(!one_wire_reset()) return -1; // Если никто не ответил — выходим

	one_wire_wr_byte(0xCC); // Skip ROM
	one_wire_wr_byte(0x44); // Start Conversion

	ds18b20.tmr = CONV_TIME_MS;

	return 1;
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

int16_t *ds18b20_get_temp(void) { return ds18b20.sensor_temperatures; }