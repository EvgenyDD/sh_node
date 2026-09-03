#include "eeprom.h"
#include "fw_header.h"
#include "i2c_common.h"
#include "platform.h"
#include "ret_mem.h"
#include "stm32f10x.h"

char g_str_dev_name[STR_DEV_LEN] = {0};
char *g_p_str_dev_name = g_str_dev_name;

uint32_t SystemCoreClock = 64000000;

static inline void delay_10ms_loop(void)
{
	// 640,000 total cycles needed / 4 cycles per loop iteration = 160,000 iterations
	uint32_t count = 160000;

	__asm volatile(
		"1: subs %0, #1 \n\t" // 1 cycle: Subtract 1 from count
		"bne 1b         \n\t" // 3 cycles if branch taken, 1 cycle if not
		: "+r"(count)
		:
		: "cc");
}

__attribute__((noreturn)) void main(void)
{
	RCC->CR |= (uint32_t)0x00000001;

	FLASH->ACR |= FLASH_ACR_PRFTBE; /* Enable Prefetch Buffer */

	/* Flash 2 wait state */
	FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);
	FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_2;

	RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;	/* HCLK = SYSCLK */
	RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1; /* PCLK2 = HCLK */
	RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV2; /* PCLK1 = HCLK */

	/*  PLL configuration: PLLCLK = HSI/2 * 16 = 64 MHz */
	RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
	RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSI_Div2 | RCC_CFGR_PLLMULL16);

	RCC->CR |= RCC_CR_PLLON; /* Enable PLL */

	while((RCC->CR & RCC_CR_PLLRDY) == 0) /* Wait till PLL is ready */
	{
	}

	/* Select PLL as system clock source */
	RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
	RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;

	while((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08) /* Wait till PLL is used as system clock source */
	{
	}

	RCC->AHBENR |= RCC_AHBENR_CRCEN;

	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN; // I2C EEP

	ret_mem_init();

	// determine load source
	load_src_t load_src = ret_mem_get_load_src();
	ret_mem_set_load_src(LOAD_SRC_NONE);

	delay_10ms_loop(); // EEP init

	i2c_init();
	eeprom_read(STR_DEV_ADDR, (uint8_t *)g_p_str_dev_name, STR_DEV_LEN);

	fw_header_check_all(g_p_str_dev_name, STR_DEV_LEN);

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