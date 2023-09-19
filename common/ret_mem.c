
#include "ret_mem.h"
#include "stm32f10x.h"

void ret_mem_init(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
	PWR->CR |= PWR_CR_DBP;
	while((PWR->CR & PWR_CR_DBP) == 0)
		;
}

load_src_t ret_mem_get_load_src(void)
{
	return BKP->DR1;
}

void ret_mem_set_load_src(load_src_t src)
{
	BKP->DR1 = src;
}
