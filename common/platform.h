#ifndef PLATFORM_H
#define PLATFORM_H

#include "stm32f10x.h"
#include <stdbool.h>
#include <stdint.h>

#define _BV(x) (1ULL << (x))

#define PAGE_SIZE (1024)

#define FLASH_LEN (0x00020000U) // 128kB
#define FLASH_START FLASH_BASE
#define FLASH_ORIGIN FLASH_BASE
#define FLASH_FINISH (FLASH_BASE + FLASH_LEN)
#define FLASH_SIZE FLASH_LEN

#define PIN_SET_(x, y) x->BSRR = (1 << y)
#define PIN_RST_(x, y) x->BSRR = ((uint32_t)((1 << y))) << 16
#define PIN_WR_(x, y, v) x->BSRR = ((uint32_t)((1 << y))) << ((!(v)) * 16)
#define PIN_GET_(x, y) !!(x->IDR & (1 << y))
#define PIN_GET_ODR_(x, y) !!(x->ODR & (1 << y))

#define PIN_SET(x) x##_Port->BSRR = x##_Pin
#define PIN_RST(x) x##_Port->BSRR = ((uint32_t)(x##_Pin)) << 16
#define PIN_WR(x, v) x##_Port->BSRR = ((uint32_t)(x##_Pin)) << ((!(v)) * 16)
#define PIN_GET(x) !!(x##_Port->IDR & x##_Pin)
#define PIN_GET_ODR(x) !!(x##_Port->ODR & x##_Pin)

#define UNIQUE_ID 0x1FFFF7E8

void platform_flash_erase_flag_reset(void);
void platform_flash_erase_flag_reset_sect_cfg(void);

void platform_flash_unlock(void);
void platform_flash_lock(void);
int platform_flash_read(uint32_t addr, uint8_t *src, uint32_t sz);
int platform_flash_write(uint32_t dest, const uint8_t *src, uint32_t sz, bool erase_sectors);

void platform_deinit(void);
void platform_reset(void);
void platform_run_address(uint32_t address);

void platform_get_uid(uint32_t *id);

void delay_ms(volatile uint32_t delay_ms);

void platform_watchdog_init(void);
static inline void platform_watchdog_reset(void) { IWDG_ReloadCounter(); }

const char *paltform_reset_cause_get(void);

void _lseek_r(void);
void _close_r(void);
void _read_r(void);
void _write_r(void);

void USART1_IRQHandler(void);

extern int __preldr_start, __preldr_end;
extern int __ldr_start, __ldr_end;
extern int __cfg_start, __cfg_end;
extern int __app_start, __app_end;
extern int __header_offset;

extern uint32_t g_uid[3];

#endif // PLATFORM_H
