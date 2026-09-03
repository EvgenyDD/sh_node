#include "dsm501.h"
#include "platform.h"

#define INTERVAL_MS 30000

dsm501_data_t dsm501_data = {0};

static uint32_t counter = 0, acc_ms = 0;

void dsm501_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void dsm501_poll(uint32_t diff_ms)
{
	counter += diff_ms;
	if(!(GPIOB->IDR & (1 << 15)))
	{
		acc_ms += diff_ms;
	}
	if(counter >= INTERVAL_MS)
	{
		dsm501_data.acc_data_ms = acc_ms;
		dsm501_data.a++;
		counter = 0;
		acc_ms = 0;
	}
}
