#include "pir.h"
#include "CANopen.h"
#include "OD.h"
#include "cfg_device.h"
#include "platform.h"

#ifdef CFG_USE_PIR

extern CO_t *CO;

#define PIR_OFF_TO_MS 20000

static uint32_t pir_off_tmr = 0;

void pir_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12; // PIR
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void pir_poll(uint32_t diff_ms)
{
	static bool pir_prev = 0;
	bool pir_now = PIN_GET_(GPIOB, 12);
	if(pir_now && !pir_prev)
	{
		pir_off_tmr = PIR_OFF_TO_MS;
		OD_RAM.x6104_pir.pir_state = 1;
		CO_TPDOsendRequest(&CO->TPDO[1]);
	}
	if(pir_off_tmr)
	{
		pir_off_tmr = pir_off_tmr > diff_ms ? pir_off_tmr - diff_ms : 0;
		if(pir_off_tmr == 0)
		{
			OD_RAM.x6104_pir.pir_state = 0;
			CO_TPDOsendRequest(&CO->TPDO[1]);
		}
	}
	pir_prev = pir_now;
}

#endif