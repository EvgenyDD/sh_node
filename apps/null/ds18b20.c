#include "ds18b20.h"
#include "platform.h"
#include "stm32f10x.h"

struct
{
	int16_t rawtemp;
	uint32_t tmr;
	uint8_t temp[2];
	uint32_t c;
} ds18b20 = {0};

static void OneWire_ChangeBaudrate(uint32_t new_baudrate)
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

static uint8_t USART1_Transfer(uint8_t value)
{
	USART_ReceiveData(USART1);
	while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
		;
	USART_SendData(USART1, value);
	while(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET)
		;
	return USART_ReceiveData(USART1);
}

static uint8_t OneWire_Reset(void)
{
	OneWire_ChangeBaudrate(9600);

	// Шлем 0xF0. Если датчик на шине есть, он прижмет линию к нулю, и прилетевший обратно байт НЕ будет равен 0xF0.
	uint8_t presence_byte = USART1_Transfer(0xF0);

	OneWire_ChangeBaudrate(115200); // Возвращаем рабочую скорость

	if(presence_byte != 0xF0)
	{
		return 1; // Датчик(и) на шине обнаружен(ы)
	}
	return 0; // Никого нет
}

static void OneWire_WriteBit(uint8_t bit)
{
	if(bit)
	{
		USART1_Transfer(0xFF); // Шлем 1
	}
	else
	{
		USART1_Transfer(0x00); // Шлем 0
	}
}

static uint8_t OneWire_ReadBit(void)
{
	// Чтобы прочитать бит, мы отпускаем линию (шлем 0xFF) Если датчик хочет сказать "0", он прижмет линию, и вернется НЕ 0xFF
	if(USART1_Transfer(0xFF) == 0xFF)
	{
		return 1;
	}
	return 0;
}

static void OneWire_WriteByte(uint8_t byte)
{
	for(uint8_t i = 0; i < 8; i++)
	{
		OneWire_WriteBit(byte & (1 << i));
	}
}

static uint8_t OneWire_ReadByte(void)
{
	uint8_t byte = 0;
	for(uint8_t i = 0; i < 8; i++)
	{
		if(OneWire_ReadBit())
		{
			byte |= (1 << i);
		}
	}
	return byte;
}

int ds18b20_read(uint32_t diff_ms)
{
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
			if(!OneWire_Reset()) return -2;

			OneWire_WriteByte(0xCC); // Skip ROM
			OneWire_WriteByte(0xBE); // Read Scratchpad (чтение памяти датчика)

			uint8_t temp_lsb = OneWire_ReadByte(); // Младший байт температуры
			uint8_t temp_msb = OneWire_ReadByte(); // Старший байт температуры
			ds18b20.temp[0] = temp_lsb;
			ds18b20.temp[1] = temp_msb;
			ds18b20.c++;

			// Переводим в нормальное число
			int16_t raw_temp = (temp_msb << 8) | temp_lsb;
			ds18b20.rawtemp = raw_temp;
			// float temperature = (float)raw_temp / 16.0;
			return 0;
		}
	}
	if(!OneWire_Reset()) return -1; // Если никто не ответил — выходим

	OneWire_WriteByte(0xCC); // Skip ROM (игнорируем адрес, если датчик один)
	OneWire_WriteByte(0x44); // Start Conversion (запуск измерения)

	ds18b20.tmr = 750;

	return 1;
}

void ds18b20_init(uint32_t baudrate)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

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