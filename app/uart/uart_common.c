#include "uart_common.h"
#include "platform.h"
#include "stm32f10x.h"

extern void gps_parse(uint8_t c);

void uart_common_init(void)
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
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	USART_InitTypeDef USART_InitStructure = {0};
	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	NVIC_EnableIRQ(USART1_IRQn);
	USART_Cmd(USART1, ENABLE);

	USART_SendData(USART1, 0);
	while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
		;
	delay_ms(1);
}

void uart_tx(const uint8_t *data, uint32_t len)
{
	for(uint32_t i = 0; i < len; i++)
	{
		USART_SendData(USART1, data[i]);
		while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
			;
	}
}

volatile uint8_t rx_buf[256] = {0};
volatile uint32_t rx_buf_ptr = 0;

void USART1_IRQHandler(void)
{
	if(USART_GetITStatus(USART1, USART_IT_RXNE))
	{
		volatile uint8_t ch = USART_ReceiveData(USART1) & 0xFF;
		if(rx_buf_ptr < sizeof(rx_buf)) rx_buf[rx_buf_ptr++] = ch;
		gps_parse(ch);
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
	NVIC_ClearPendingIRQ(USART1_IRQn);
}