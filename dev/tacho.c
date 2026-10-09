#include "tacho.h"
#include "CANopen.h"
#include "OD.h"
#include "cfg_device.h"

// 0 - A8  - TIM1 CH1
// 1 - A7  - TIM3 CH2
// 2 - B6  - TIM4 CH1
// 3 - A15 - TIM2 CH1

#if defined(CFG_USE_TACHO0) || defined(CFG_USE_TACHO1) || defined(CFG_USE_TACHO2) || defined(CFG_USE_TACHO3)

#define MAX_INTRVL_MS 2500

static uint32_t tacho_prev[4] = {0}, tacho_time[4] = {0};

void tacho_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
	TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
	TIM_TimeBaseStructure.TIM_Prescaler = 0;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_ICInitTypeDef TIM_ICInitStructure = {0};

#ifdef CFG_USE_TACHO0
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
	TIM_ICInitStructure.TIM_ICFilter = 0x4;
	TIM_ICInit(TIM1, &TIM_ICInitStructure);

	TIM_TIxExternalClockConfig(TIM1, TIM_TIxExternalCLK1Source_TI1, TIM_ICPolarity_Rising, 0x4);

	TIM_SetCounter(TIM1, 0);
	TIM_Cmd(TIM1, ENABLE);
#endif // CFG_USE_TACHO0

#ifdef CFG_USE_TACHO1
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

	TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
	TIM_ICInitStructure.TIM_ICFilter = 0x4;
	TIM_ICInit(TIM3, &TIM_ICInitStructure);

	TIM_TIxExternalClockConfig(TIM3, TIM_TIxExternalCLK1Source_TI2, TIM_ICPolarity_Rising, 0x4);

	TIM_SetCounter(TIM3, 0);
	TIM_Cmd(TIM3, ENABLE);
#endif // CFG_USE_TACHO1

#ifdef CFG_USE_TACHO2
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
	TIM_ICInitStructure.TIM_ICFilter = 0x4;
	TIM_ICInit(TIM4, &TIM_ICInitStructure);

	// TIM_ITRxExternalClockConfig(TIM4, TIM_TS_TI1F_ED);
	TIM_TIxExternalClockConfig(TIM4, TIM_TIxExternalCLK1Source_TI1, TIM_ICPolarity_Rising, 0x4);

	TIM_SetCounter(TIM4, 0);
	TIM_Cmd(TIM4, ENABLE);
#endif // CFG_USE_TACHO2

#ifdef CFG_USE_TACHO3
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	GPIO_PinRemapConfig(GPIO_PartialRemap1_TIM2, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
	TIM_ICInitStructure.TIM_ICFilter = 0x4;
	TIM_ICInit(TIM2, &TIM_ICInitStructure);

	TIM_TIxExternalClockConfig(TIM2, TIM_TIxExternalCLK1Source_TI1, TIM_ICPolarity_Rising, 0x4);

	TIM_SetCounter(TIM2, 0);
	TIM_Cmd(TIM2, ENABLE);
#endif // CFG_USE_TACHO3
}

void tacho_poll(uint32_t diff_ms)
{
	if(diff_ms == 0) return;
	uint32_t tacho_now[4] = {TIM1->CNT, TIM3->CNT, TIM4->CNT, TIM2->CNT};
	uint32_t delta;

#define T(N)                                                    \
	tacho_time[N] += diff_ms;                                   \
	if(tacho_now[N] != tacho_prev[N])                           \
	{                                                           \
		delta = (uint32_t)(tacho_now[N] - tacho_prev[N]);       \
		uint32_t v = delta * 100 * 1000 / tacho_time[N];        \
		OD_RAM.x6105_tacho[N] = v >= INT16_MAX ? INT16_MAX : v; \
		OD_RAM.x6106_tacho_acc[N] += delta;                     \
		tacho_prev[N] = tacho_now[N];                           \
		tacho_time[N] = 0;                                      \
	}                                                           \
	if(tacho_time[N] > MAX_INTRVL_MS)                           \
	{                                                           \
		OD_RAM.x6105_tacho[N] = 0;                              \
		tacho_time[N] = MAX_INTRVL_MS;                          \
	}

#ifdef CFG_USE_TACHO0
	T(0);
#endif

#ifdef CFG_USE_TACHO1
	T(1);
#endif

#ifdef CFG_USE_TACHO2
	T(2);
#endif

#ifdef CFG_USE_TACHO3
	T(3);
#endif
}

#endif
