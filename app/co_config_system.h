#ifndef CO_CONFIG_SYSTEM_H__
#define CO_CONFIG_SYSTEM_H__

#define CO_CONFIG_SDO_SRV          \
	(CO_CONFIG_SDO_SRV_SEGMENTED | \
	 CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE)

#define CO_CONFIG_SDO_SRV_BUFFER_SIZE 534

#define CO_CONFIG_LSS      \
	(CO_CONFIG_LSS_SLAVE | \
	 CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE)

#endif // CO_CONFIG_SYSTEM_H__