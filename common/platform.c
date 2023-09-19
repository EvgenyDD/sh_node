#include "platform.h"
#include <string.h>

static uint8_t page_erased_bits[FLASH_SIZE / PAGE_SIZE] = {0};

static int flash_wait_op(void)
{
	int error = 0;
	while((FLASH->SR & FLASH_SR_BSY) == FLASH_SR_BSY)
	{
	}

	if(FLASH->SR & FLASH_SR_EOP) FLASH->SR = FLASH_SR_EOP;

	if(FLASH->SR & FLASH_SR_WRPRTERR)
	{
		FLASH->SR = FLASH_SR_WRPRTERR;
		error = 5;
	}
	if(FLASH->SR & FLASH_SR_PGERR)
	{
		FLASH->SR = FLASH_SR_PGERR;
		error = 6;
	}

	return error;
}

static int erase_page(uint32_t dest)
{
	FLASH->CR |= FLASH_CR_PER;
	FLASH->AR = dest;
	FLASH->CR |= FLASH_CR_STRT;
	int sts = flash_wait_op();
	FLASH->CR &= ~(FLASH_CR_PER);
	return sts;
}

void platform_flash_erase_flag_reset(void)
{
	memset(page_erased_bits, 0, sizeof(page_erased_bits));
}

void platform_flash_erase_flag_reset_sect_cfg(void)
{
	page_erased_bits[(((uint32_t)&__cfg_start) - (FLASH_ORIGIN)) / PAGE_SIZE] = 0;
}

void platform_flash_unlock(void)
{
	FLASH_Unlock();
}

void platform_flash_lock(void)
{
#define CR_LOCK_Set ((uint32_t)0x00000080)
	FLASH->CR |= CR_LOCK_Set;
}

int platform_flash_write(uint32_t dest, const uint8_t *src, uint32_t sz, bool erase_sectors)
{
	if(sz == 0) return 0;
	if(sz & 1) return 1; // program only by half-word

	uint32_t page = (dest - FLASH_BASE) / PAGE_SIZE;
	if(page > sizeof(page_erased_bits)) return 2;

	platform_flash_unlock();
	if(page_erased_bits[page] == 0 && erase_sectors)
	{
		if(erase_page(dest))
		{
			platform_flash_lock();
			return 3;
		}
		page_erased_bits[page] = 1;
	}

	int sts = flash_wait_op();
	if(sts)
	{
		platform_flash_lock();
		return sts;
	}

	FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR;

	uint16_t halfword;
	for(volatile uint32_t i = 0; i < (sz >> 1U); i++)
	{
		FLASH->CR |= FLASH_CR_PG;

		memcpy(&halfword, &src[i << 1U], 2);
		*(__IO uint16_t *)(dest + (i << 1U)) = halfword;

		sts = flash_wait_op();
		FLASH->CR &= ~(FLASH_CR_PG);
		if(sts)
		{
			platform_flash_lock();
			return sts;
		}

		if(*(__IO uint16_t *)(dest + (i << 1U)) != halfword)
		{
			platform_flash_lock();
			return 5;
		}
	}
	platform_flash_lock();
	return 0;
}

int platform_flash_read(uint32_t addr, uint8_t *src, uint32_t sz)
{
	if(addr < FLASH_START || addr + sz >= FLASH_FINISH) return 1;
	memcpy(src, (void *)addr, sz);
	return 0;
}

void platform_deinit(void)
{
	platform_flash_lock();
	__disable_irq();
	SysTick->CTRL = 0;
	for(uint32_t i = 0; i < sizeof(NVIC->ICPR) / sizeof(NVIC->ICPR[0]); i++)
	{
		NVIC->ICPR[i] = 0xfffffffflu;
	}
	__enable_irq();
}

void platform_reset(void)
{
	platform_deinit();
	NVIC_SystemReset();
}

typedef void (*pFunction)(void);
uint32_t JumpAddress;
pFunction Jump_To_Application;

__attribute__((optimize("-O0"))) __attribute__((always_inline)) static __inline void boot_jump(uint32_t address)
{
	JumpAddress = *(__IO uint32_t *)(address + 4);
	Jump_To_Application = (pFunction)JumpAddress;
	__set_MSP(*(__IO uint32_t *)address);
	Jump_To_Application();
}

__attribute__((optimize("-O0"))) void platform_run_address(uint32_t address)
{
	platform_deinit();
	SCB->VTOR = address;
	boot_jump(address);
}

void platform_get_uid(uint32_t *id)
{
	memcpy(id, (void *)UNIQUE_ID, 3 * sizeof(uint32_t));
}

void platform_watchdog_init(void)
{
	DBGMCU_Config(DBGMCU_IWDG_STOP, ENABLE);
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
	IWDG_SetPrescaler(IWDG_Prescaler_256);
	IWDG_SetReload(781); // (1/40K) * 256 * VAL = X sec
	IWDG_ReloadCounter();
	IWDG_Enable();
}