
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

void ret_mem_set_rst_cause_ldr(uint32_t src)
{
	BKP->DR2 = src;
}

void ret_mem_set_rst_cause_app(uint32_t src)
{
	BKP->DR3 = src;
}

uint32_t ret_mem_get_rst_cause_ldr(void)
{
	return BKP->DR2;
}

uint32_t ret_mem_get_rst_cause_app(void)
{
	return BKP->DR3;
}
