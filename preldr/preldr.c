#include "fw_header.h"
#include "platform.h"
#include "ret_mem.h"
#include "stm32f10x.h"

void main(void)
{
	RCC->CR |= (uint32_t)0x00000001;

	/* Enable Prefetch Buffer */
	FLASH->ACR |= FLASH_ACR_PRFTBE;

	/* Flash 2 wait state */
	FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);
	FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_2;

	/* HCLK = SYSCLK */
	RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;

	/* PCLK2 = HCLK */
	RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;

	/* PCLK1 = HCLK */
	RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV2;

	/*  PLL configuration: PLLCLK = HSI/2 * 16 = 64 MHz */
	RCC->CFGR &= (uint32_t)((uint32_t) ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
	RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSI_Div2 | RCC_CFGR_PLLMULL16);

	/* Enable PLL */
	RCC->CR |= RCC_CR_PLLON;

	/* Wait till PLL is ready */
	while((RCC->CR & RCC_CR_PLLRDY) == 0)
	{
	}

	/* Select PLL as system clock source */
	RCC->CFGR &= (uint32_t)((uint32_t) ~(RCC_CFGR_SW));
	RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;

	/* Wait till PLL is used as system clock source */
	while((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08)
	{
	}

	RCC->AHBENR |= RCC_AHBENR_CRCEN;

	ret_mem_init();

	// determine load source
	load_src_t load_src = ret_mem_get_load_src();
	ret_mem_set_load_src(LOAD_SRC_NONE);

	fw_header_check_all();

	// force goto app -> cause rebooted from bootloader
	if(load_src == LOAD_SRC_BOOTLOADER)
	{
		if(g_fw_info[FW_APP].locked == false)
		{
			platform_run_address((uint32_t)&__app_start);
		}
	}

	// run bootloader
	if(g_fw_info[FW_LDR].locked == false)
	{
		platform_run_address((uint32_t)&__ldr_start);
	}

	// load src not bootloader && bootloader is corrupt
	if(g_fw_info[FW_APP].locked == false)
	{
		platform_run_address((uint32_t)&__app_start);
	}

	while(1)
	{
	}
}