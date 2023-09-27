#include "platform.h"

#define TO_ms 10000

static uint32_t delay_counter = 0;

void pir_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void pir_poll(uint32_t diff_ms)
{
	if(GPIOB->IDR & (1 << 12))
	{
		GPIOA->BSRR = (1 << 1);
		delay_counter = TO_ms;
	}
	else
	{
		if(delay_counter > diff_ms)
		{
			delay_counter -= diff_ms;
			if((delay_counter % 300) > 200)
				GPIOA->BSRR = (1 << 1);
			else
				GPIOA->BRR = (1 << 1);
		}
		else
		{
			delay_counter = 0;
			GPIOA->BRR = (1 << 1);
		}
	}
}