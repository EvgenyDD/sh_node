
#include "CANopen.h"
#include "CO_driver_app.h"
#include "CO_driver_storage.h"
#include "OD.h"
#include "adc.h"
#include "aht21.h"
#include "apps.h"
#include "baro.h"
#include "can_driver.h"
#include "config_system.h"
#include "crc.h"
#include "dsm501.h"
#include "flasher_sdo.h"
#include "fw_header.h"
#include "gps.h"
#include "i2c_common.h"
#include "lss_cb.h"
#include "mag.h"
#include "platform.h"
#include "prof.h"
#include "ret_mem.h"
#include "roles.h"
#include "spi_common.h"
#include "uart_common.h"

#define SYSTICK_IN_US (64000000 / 1000000)
#define SYSTICK_IN_MS (64000000 / 1000)

#define NMT_CONTROL                  \
	(CO_NMT_STARTUP_TO_OPERATIONAL | \
	 CO_NMT_ERR_ON_ERR_REG |         \
	 CO_NMT_ERR_ON_BUSOFF_HB |       \
	 CO_ERR_REG_GENERIC_ERR |        \
	 CO_ERR_REG_COMMUNICATION)

bool g_stay_in_boot = false;
uint32_t g_uid[3];
CO_t *CO = NULL;

extern void pir_init(void);
extern void pir_poll(uint32_t diff_ms);

uint8_t g_active_can_node_id = 127;		  /* Copied from CO_pending_can_node_id in the communication reset section */
static uint8_t pending_can_node_id = 127; /* read from dip switches or nonvolatile memory, configurable by LSS slave */
static uint16_t pending_can_baud = 500;	  /* read from dip switches or nonvolatile memory, configurable by LSS slave */

// CO_errorReport(CO->em, 0x05, CO_EMC_DEVICE_SPECIFIC, 0);
static int32_t prev_systick = 0;

config_entry_t g_device_config[] = {
	{"can_id", sizeof(pending_can_node_id), 0, &pending_can_node_id},
	{"can_baud", sizeof(pending_can_baud), 0, &pending_can_baud},
	{"hb_prod_ms", sizeof(OD_PERSIST_COMM.x1017_producerHeartbeatTime), 0, &OD_PERSIST_COMM.x1017_producerHeartbeatTime},
	{"dev_role", sizeof(OD_PERSIST_COMM.x1000_deviceType), 0, &OD_PERSIST_COMM.x1000_deviceType},
};
const uint32_t g_device_config_count = sizeof(g_device_config) / sizeof(g_device_config[0]);

void delay_ms(volatile uint32_t delay_ms)
{
	volatile uint32_t start = 0;
	int32_t mark_prev = 0;
	prof_mark(&mark_prev);
	const uint32_t time_limit = delay_ms * SYSTICK_IN_MS;
	for(;;)
	{
		start += (uint32_t)prof_mark(&mark_prev);
		if(start >= time_limit)
			return;
	}
}

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

	platform_get_uid(g_uid);

	prof_init();
	platform_watchdog_init();

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO |
							   RCC_APB2Periph_GPIOA |
							   RCC_APB2Periph_GPIOB |
							   RCC_APB2Periph_GPIOC |
							   RCC_APB2Periph_GPIOD,
						   ENABLE);

	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_PD01, ENABLE);

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOC, &GPIO_InitStruct);

	GPIOC->BSRR = (1 << 14);

	fw_header_check_all();

	ret_mem_init();
	ret_mem_set_load_src(LOAD_SRC_APP); // let preboot know it was booted from bootloader
	spi_common_init();
	baro_init();
	adc_init();
	i2c_common_init();

	can_drv_init(CAN1);

	CO = CO_new(NULL, (uint32_t[]){0});
	CO_driver_storage_init(OD_ENTRY_H1010_storeParameters, OD_ENTRY_H1011_restoreDefaultParameters);
	co_od_init_headers();
	flasher_sdo_init();

	OD_PERSIST_COMM.x1016_consumerHeartbeatTime[0] = (1 /* master ID */ << 16) | 5000;

	aht21_init();
	uart_common_init();

	if(OD_PERSIST_COMM.x1000_deviceType != ND_ROLE_METEO) dsm501_init();
	if(OD_PERSIST_COMM.x1000_deviceType != ND_ROLE_METEO) pir_init();

	if(OD_PERSIST_COMM.x1000_deviceType == ND_ROLE_METEO) meteo_init();

	mag_init();
	for(;;)
	{
		LSS_cb_obj_t lss_obj = {.lss_br_set_delay_counter = 0, .co = CO};
		CO->CANmodule->CANptr = CAN1;
		CO_NMT_reset_cmd_t reset = CO_RESET_NOT;

		while(reset != CO_RESET_APP)
		{
			CO->CANmodule->CANnormal = false;

			CO_CANsetConfigurationMode(CO->CANmodule->CANptr);
			CO_CANmodule_disable(CO->CANmodule);
			if(CO_CANinit(CO, CO->CANmodule->CANptr, pending_can_baud) != CO_ERROR_NO) return;

			CO_LSS_address_t lssAddress = {.identity = {.vendorID = OD_PERSIST_COMM.x1018_identity.serialNumber,
														.productCode = OD_PERSIST_COMM.x1018_identity.UID0,
														.revisionNumber = OD_PERSIST_COMM.x1018_identity.UID1,
														.serialNumber = OD_PERSIST_COMM.x1018_identity.UID2}};

			if(CO_LSSinit(CO, &lssAddress, &pending_can_node_id, &pending_can_baud) != CO_ERROR_NO) return;
			lss_cb_init(&lss_obj);

			g_active_can_node_id = pending_can_node_id;
			uint32_t errInfo = 0;
			CO_ReturnError_t err = CO_CANopenInit(CO,		   /* CANopen object */
												  NULL,		   /* alternate NMT */
												  NULL,		   /* alternate em */
												  OD,		   /* Object dictionary */
												  NULL,		   /* Optional OD_statusBits */
												  NMT_CONTROL, /* CO_NMT_control_t */
												  500,		   /* firstHBTime_ms */
												  1000,		   /* SDOserverTimeoutTime_ms */
												  500,		   /* SDOclientTimeoutTime_ms */
												  false,	   /* SDOclientBlockTransfer */
												  g_active_can_node_id,
												  &errInfo);

			CO->em->errorStatusBits = OD_RAM.x2000_errorBits;
			if(err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) return;

			err = CO_CANopenInitPDO(CO, CO->em, OD, g_active_can_node_id, &errInfo);
			if(err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) return;

			CO_CANsetNormalMode(CO->CANmodule);
			CO_driver_storage_error_report(CO->em);

			reset = CO_RESET_NOT;

			prof_mark(&prev_systick);

			while(reset == CO_RESET_NOT)
			{
				// time diff
				uint32_t time_diff_systick = (uint32_t)prof_mark(&prev_systick);

				static uint32_t remain_systick_us_prev = 0, remain_systick_ms_prev = 0;
				uint32_t diff_us = (time_diff_systick + remain_systick_us_prev) / (SYSTICK_IN_US);
				remain_systick_us_prev = (time_diff_systick + remain_systick_us_prev) % SYSTICK_IN_US;

				uint32_t diff_ms = (time_diff_systick + remain_systick_ms_prev) / (SYSTICK_IN_MS);
				remain_systick_ms_prev = (time_diff_systick + remain_systick_ms_prev) % SYSTICK_IN_MS;

				CO_CANinterrupt(CO->CANmodule);
				reset = CO_process(CO, false, diff_us, NULL);
				lss_cb_poll(&lss_obj, diff_us);

				static uint32_t difff = 0;
				difff += diff_ms;
				if(difff >= 1500) difff = 0;

				if(difff < 5)
				{
					GPIOD->BSRR = (1 << 1);
				}
				else
				{
					GPIOD->BSRR = (1 << (1 + 16));
				}

				adc_track();
				if(aht21_data.sensor_present) aht21_poll(diff_ms);
				if(OD_PERSIST_COMM.x1000_deviceType == ND_ROLE_OUTDOOR ||
				   OD_PERSIST_COMM.x1000_deviceType == ND_ROLE_METEO)
				{
					if(baro_data.sensor_present) baro_poll(diff_ms);
				}

				if(OD_PERSIST_COMM.x1000_deviceType == ND_ROLE_METEO)
				{
					meteo_poll(diff_ms);
				}
				else
				{
					dsm501_poll(diff_ms);
					pir_poll(diff_ms);
				}
				platform_watchdog_reset();
			}
		}

	PLATFORM_RESET:
		CO_CANsetConfigurationMode(CO->CANmodule->CANptr);
		CO_delete(CO);

		platform_reset();
	}
}

void assert_failed(uint8_t *file, uint32_t line)
{
	while(1)
		;
}