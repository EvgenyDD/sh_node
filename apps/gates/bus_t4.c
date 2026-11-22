#include "bus_t4.h"
#include "CANopen.h"
#include "OD.h"
#include "platform.h"
#include "stm32f10x.h"
#include <stdbool.h>

#define RX_FIFO_SIZE 256

#define BREAK_MASK 0xFFFF

#define PREAMBLE 0x55

bus_t4_t bus = {0};

static struct
{
	volatile uint16_t data[RX_FIFO_SIZE];
	volatile uint32_t head;
	volatile uint32_t tail;
} rx_fifo = {0};

static volatile bool tx_busy = false;
static volatile uint8_t tx_buf[256];

static void uart_tx_dma(const volatile uint8_t *data, uint32_t len)
{
	if(len == 0) return;

	DMA_Cmd(DMA1_Channel4, DISABLE);
	DMA1_Channel4->CMAR = (uint32_t)data;
	DMA1_Channel4->CNDTR = len;
	DMA_ClearFlag(DMA1_FLAG_TC4 | DMA1_FLAG_HT4 | DMA1_FLAG_TE4 | DMA1_FLAG_GL4);
	DMA_Cmd(DMA1_Channel4, ENABLE);

	USART_ClearITPendingBit(USART1, USART_IT_TC);
	USART_ITConfig(USART1, USART_IT_TC, ENABLE);
	bus.ok_tx++;
}

static void delay_us_timer(volatile uint16_t us)
{
	TIM_SetCounter(TIM3, 0);
	while(TIM_GetCounter(TIM3) < us)
		asm("nop");
}

void bus_t4_send(uint8_t *data, uint16_t length)
{
	if(tx_busy) return;
	tx_busy = true;

	uint32_t idx = 0;
	tx_buf[idx++] = 0x55;
	tx_buf[idx++] = length;
	memcpy_volatile(&tx_buf[idx], data, length);
	idx += length;

	uint8_t calculated_crc = 0;
	for(uint16_t i = 1; i < idx; i++)
	{
		calculated_crc ^= tx_buf[i];
	}
	tx_buf[idx++] = calculated_crc;

	USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);

	GPIO_InitTypeDef GPIO_InitStructure = {0};
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_ResetBits(GPIOB, GPIO_Pin_6);
	delay_us_timer(519); // BREAK

	GPIO_SetBits(GPIOB, GPIO_Pin_6);
	delay_us_timer(52); // pause 1 bit

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	uart_tx_dma(tx_buf, idx);
}

void bus_t4_init(void)
{
	// B6 tx B7 rx
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_PinRemapConfig(GPIO_Remap_USART1, ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_TimeBaseStructure.TIM_Prescaler = 64 - 1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

	TIM_Cmd(TIM3, ENABLE);

	DMA_InitTypeDef DMA_InitStructure = {0};
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(USART1->DR);
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)0;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
	DMA_InitStructure.DMA_BufferSize = 0;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel4, &DMA_InitStructure);

	DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, ENABLE);

	USART_InitTypeDef USART_InitStructure = {0};
	USART_InitStructure.USART_BaudRate = 19200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	USART_LINBreakDetectLengthConfig(USART1, USART_LINBreakDetectLength_11b);

	USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_ITConfig(USART1, USART_IT_TC, DISABLE);
	USART_ITConfig(USART1, USART_IT_LBD, ENABLE); // break;

	NVIC_InitTypeDef NVIC_InitStructure = {0};
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_Cmd(USART1, ENABLE);
}

volatile struct
{
	uint8_t buf[RX_FIFO_SIZE];
	uint32_t idx;
	bool break_found;
} parser = {0};

static void parser_reset(void)
{
	parser.idx = 0;
	parser.break_found = false;
}

void bus_t4_poll(uint32_t diff_ms)
{
	while(rx_fifo.head != rx_fifo.tail)
	{
		volatile uint32_t next_tail = (rx_fifo.tail + 1) % RX_FIFO_SIZE;
		volatile uint16_t b = rx_fifo.data[rx_fifo.tail];
		rx_fifo.tail = next_tail;

		if(parser.break_found == false)
		{
			if(b == BREAK_MASK) parser.break_found = true;
		}
		else
		{
			if(b == BREAK_MASK) continue;

			parser.buf[parser.idx++] = b;
			if(parser.idx >= RX_FIFO_SIZE)
			{
				parser_reset();
			}
			else
			{
				if(parser.idx > 1)
				{
					uint8_t expected_len = parser.buf[1] + 3;
					if(parser.idx >= expected_len)
					{
						if(parser.idx > 3 && parser.buf[0] == PREAMBLE)
						{
							uint8_t calculated_crc = 0;
							for(uint16_t i = 1; i < parser.idx - 1; i++)
							{
								calculated_crc ^= parser.buf[i];
							}

							if(calculated_crc == parser.buf[parser.idx - 1])
							{
								bus.ok_rx++;

								{ // copy pkt

									static uint32_t pkt_ptr = 0;
									memset(OD_RAM.x6110_bus_t4_pkt_raw[pkt_ptr], 0, sizeof(OD_RAM.x6110_bus_t4_pkt_raw[0]));
									uint8_t len_copy = sizeof(OD_RAM.x6110_bus_t4_pkt_raw[pkt_ptr]) - 1;
									OD_RAM.x6110_bus_t4_pkt_raw[pkt_ptr][0] = parser.idx;
									if(len_copy > parser.idx) len_copy = parser.idx;
									for(uint32_t i = 0; i < len_copy; i++)
									{
										OD_RAM.x6110_bus_t4_pkt_raw[pkt_ptr][1 + i] = parser.buf[i];
									}
									if(++pkt_ptr >= OD_CNT_ARR_6110) pkt_ptr = 0;
								}
							}
							else
							{
								bus.err_crc++;
							}
						}
						else
						{
							bus.err_pkt_short++;
						}

						parser_reset();
					}
				}
			}
		}
	}
}

void USART1_IRQHandler(void)
{
	if(USART_GetITStatus(USART1, USART_IT_RXNE))
	{
		volatile uint8_t ch = (uint8_t)(USART_ReceiveData(USART1) & 0xFF);

		volatile uint32_t next_head = (rx_fifo.head + 1) % RX_FIFO_SIZE;
		if(next_head != rx_fifo.tail)
		{
			rx_fifo.data[rx_fifo.head] = ch;
			rx_fifo.head = next_head;
		}
		else
		{
			bus.err_ovf++;
		}

		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}

	if(USART_GetITStatus(USART1, USART_IT_TC))
	{
		USART_ITConfig(USART1, USART_IT_TC, DISABLE);

		volatile uint8_t dummy = USART_ReceiveData(USART1);
		(void)dummy;
		USART_ClearFlag(USART1, USART_FLAG_RXNE);

		USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

		tx_busy = false;
	}

	if(USART_GetITStatus(USART1, USART_IT_LBD) != RESET)
	{
		volatile uint32_t next_head = (rx_fifo.head + 1) % RX_FIFO_SIZE;
		if(next_head != rx_fifo.tail)
		{
			rx_fifo.data[rx_fifo.head] = BREAK_MASK;
			rx_fifo.head = next_head;
		}
		else
		{
			bus.err_ovf++;
		}

		if(USART_GetFlagStatus(USART1, USART_FLAG_FE) != RESET)
		{
			volatile uint32_t dummy = USART1->DR;
			(void)dummy;
		}

		USART_ClearITPendingBit(USART1, USART_IT_LBD);
	}

	NVIC_ClearPendingIRQ(USART1_IRQn);
}
