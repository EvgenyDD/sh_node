// single include file

#include "CANopen.h"
#include "CO_driver_app.h"
#include "CO_driver_storage.h"
#include "OD.h"
#include "adc.h"
#include "can_driver.h"
#include "config_system.h"
#include "crc.h"
#include "debounce.h"
#include "dev/aht21.h"
#include "dev/baro.h"
#include "dev/ds18b20.h"
#include "dev/dsm501.h"
#include "dev/pir.h"
#include "dev/spi_common.h"
#include "dev/tacho.h"
#include "eeprom.h"
#include "flasher_sdo.h"
#include "fw_header.h"
#include "i2c_common.h"
#include "lss_cb.h"
#include "platform.h"
#include "prof.h"
#include "ret_mem.h"

#define NMT_CONTROL                  \
	(CO_NMT_STARTUP_TO_OPERATIONAL | \
	 CO_NMT_ERR_ON_BUSOFF_HB |       \
	 CO_ERR_REG_GENERIC_ERR |        \
	 CO_ERR_REG_COMMUNICATION)

bool g_stay_in_boot = false;
uint32_t g_uid[3];
CO_t *CO = NULL;

char g_str_dev_name[STR_DEV_LEN] = {0};
char *g_p_str_dev_name = g_str_dev_name;

uint8_t g_active_can_node_id = 127;
static uint8_t pending_can_node_id = 127;
static uint16_t pending_can_baud = 500;

#define CONFIG_GENERIC_INIT()                                                                                                 \
	config_entry_t g_device_config[] = {                                                                                      \
		{"can_id", sizeof(pending_can_node_id), 0, &pending_can_node_id},                                                     \
		{"can_baud", sizeof(pending_can_baud), 0, &pending_can_baud},                                                         \
		{"hb_prod_ms", sizeof(OD_PERSIST_COMM.x1017_producerHeartbeatTime), 0, &OD_PERSIST_COMM.x1017_producerHeartbeatTime}, \
	};                                                                                                                        \
	const uint32_t g_device_config_count = sizeof(g_device_config) / sizeof(g_device_config[0])

#define GENERIC_INIT_0()     \
	platform_init_clocks();  \
	platform_init();         \
	platform_get_uid(g_uid); \
	prof_init();             \
	platform_watchdog_init();

#define GENERIC_INIT()                                                                               \
	fw_header_eep_od_init();                                                                         \
	fw_header_check_all(g_p_str_dev_name, STR_DEV_LEN);                                              \
	ret_mem_init();                                                                                  \
	ret_mem_set_rst_cause_app(platform_handle_reset_cause());                                        \
	ret_mem_set_load_src(LOAD_SRC_APP); /* let preboot know it was booted from app */                \
	can_drv_init(CAN1);                                                                              \
	CO = CO_new(NULL, (uint32_t[]){0});                                                              \
	CO_driver_storage_init(OD_ENTRY_H1010_storeParameters, OD_ENTRY_H1011_restoreDefaultParameters); \
	co_od_init_headers();                                                                            \
	flasher_sdo_init();

#define CAN_LOOP_PRE()                                                                                       \
	LSS_cb_obj_t lss_obj = {.lss_br_set_delay_counter = 0, .co = CO};                                        \
	CO->CANmodule->CANptr = CAN1;                                                                            \
	CO_NMT_reset_cmd_t reset = CO_RESET_NOT;                                                                 \
                                                                                                             \
	while(reset != CO_RESET_APP)                                                                             \
	{                                                                                                        \
		CO->CANmodule->CANnormal = false;                                                                    \
                                                                                                             \
		CO_CANsetConfigurationMode(CO->CANmodule->CANptr);                                                   \
		CO_CANmodule_disable(CO->CANmodule);                                                                 \
		if(CO_CANinit(CO, CO->CANmodule->CANptr, pending_can_baud) != CO_ERROR_NO) return;                   \
                                                                                                             \
		CO_LSS_address_t lssAddress = {.identity = {.vendorID = OD_PERSIST_COMM.x1018_identity.serialNumber, \
													.productCode = OD_PERSIST_COMM.x1018_identity.UID0,      \
													.revisionNumber = OD_PERSIST_COMM.x1018_identity.UID1,   \
													.serialNumber = OD_PERSIST_COMM.x1018_identity.UID2}};   \
                                                                                                             \
		if(CO_LSSinit(CO, &lssAddress, &pending_can_node_id, &pending_can_baud) != CO_ERROR_NO) return;      \
		lss_cb_init(&lss_obj);                                                                               \
                                                                                                             \
		g_active_can_node_id = pending_can_node_id;                                                          \
		uint32_t errInfo = 0;                                                                                \
		CO_ReturnError_t err = CO_CANopenInit(CO,		   /* CANopen object */                              \
											  NULL,		   /* alternate NMT */                               \
											  NULL,		   /* alternate em */                                \
											  OD,		   /* Object dictionary */                           \
											  NULL,		   /* Optional OD_statusBits */                      \
											  NMT_CONTROL, /* CO_NMT_control_t */                            \
											  500,		   /* firstHBTime_ms */                              \
											  1000,		   /* SDOserverTimeoutTime_ms */                     \
											  500,		   /* SDOclientTimeoutTime_ms */                     \
											  false,	   /* SDOclientBlockTransfer */                      \
											  g_active_can_node_id,                                          \
											  &errInfo);                                                     \
                                                                                                             \
		CO->em->errorStatusBits = OD_RAM.x2000_errorBits;                                                    \
		if(err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) return;                           \
                                                                                                             \
		err = CO_CANopenInitPDO(CO, CO->em, OD, g_active_can_node_id, &errInfo);                             \
		if(err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) return;                           \
                                                                                                             \
		CO_CANsetNormalMode(CO->CANmodule);                                                                  \
		CO_driver_storage_error_report(CO->em);                                                              \
                                                                                                             \
		reset = CO_RESET_NOT;                                                                                \
                                                                                                             \
		static int32_t prev_systick = 0;                                                                     \
		prof_mark(&prev_systick);                                                                            \
                                                                                                             \
		while(reset == CO_RESET_NOT)                                                                         \
		{                                                                                                    \
			uint32_t time_diff_systick = (uint32_t)prof_mark(&prev_systick);                                 \
                                                                                                             \
			static uint32_t remain_systick_us_prev = 0, remain_systick_ms_prev = 0;                          \
			uint32_t diff_us = (time_diff_systick + remain_systick_us_prev) / (SYSTICK_IN_US);               \
			remain_systick_us_prev = (time_diff_systick + remain_systick_us_prev) % SYSTICK_IN_US;           \
                                                                                                             \
			uint32_t diff_ms = (time_diff_systick + remain_systick_ms_prev) / (SYSTICK_IN_MS);               \
			remain_systick_ms_prev = (time_diff_systick + remain_systick_ms_prev) % SYSTICK_IN_MS;           \
                                                                                                             \
			platform_watchdog_reset();                                                                       \
                                                                                                             \
			CO_CANinterrupt(CO->CANmodule);                                                                  \
			reset = CO_process(CO, false, diff_us, NULL);                                                    \
			bool sync_was = CO_process_SYNC(CO, diff_us, NULL);                                              \
			CO_process_TPDO(CO, sync_was, diff_us, NULL);                                                    \
			CO_process_RPDO(CO, sync_was, diff_us, NULL);                                                    \
			lss_cb_poll(&lss_obj, diff_us);

#define CAN_LOOP_POST()                                \
	}                                                  \
	}                                                  \
                                                       \
	PLATFORM_RESET:                                    \
	CO_CANsetConfigurationMode(CO->CANmodule->CANptr); \
	CO_delete(CO);                                     \
	platform_reset();

#define GENERIC_LED_FLASH()         \
	static uint32_t led_tim = 0;    \
	led_tim += diff_ms;             \
	if(led_tim >= 500) led_tim = 0; \
	PIN_WR_(GPIOD, 1, led_tim < 5);
