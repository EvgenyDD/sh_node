#include "CO_driver_app.h"
#include "CANopen.h"
#include "OD.h"
#include "fw_header.h"
#include "platform.h"
#include <string.h>

extern CO_t *CO;

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

static inline int count_digits(uint32_t value)
{
	int result = 0;
	while(value != 0)
	{
		value /= 10;
		result++;
	}
	return result == 0 ? 1 : result;
}

static inline void _print_num(char *buf, int *ptr, int sz, uint32_t num)
{
	int cnt = count_digits(num);
	if(*ptr > sz - 1 - cnt) return;
	uint32_t n = num;
	for(int i = 0; i < cnt; i++)
	{
		buf[*ptr + cnt - i - 1] = (n % 10) + '0';
		n /= 10;
	}
	*ptr += cnt;
}

void co_od_init_headers(void)
{
	memset(OD_PERSIST_COMM.x1009_manufacturerHardwareVersion, 0, sizeof(OD_PERSIST_COMM.x1009_manufacturerHardwareVersion));
	memset(OD_PERSIST_COMM.x1008_manufacturerDeviceName, 0, sizeof(OD_PERSIST_COMM.x1008_manufacturerDeviceName));
	memset(OD_PERSIST_COMM.x100A_manufacturerSoftwareVersion, 0, sizeof(OD_PERSIST_COMM.x100A_manufacturerSoftwareVersion));
	memset(OD_PERSIST_COMM.x1018_identity.buildTimedate, 0, sizeof(OD_PERSIST_COMM.x1018_identity.buildTimedate));

	/// 0x1008 manufacturerDeviceName
	if(g_fw_info[FW_TYPE].field_product_name_ptr &&
	   g_fw_info[FW_TYPE].field_product_name_len > 0)
	{
		memcpy(OD_PERSIST_COMM.x1008_manufacturerDeviceName,
			   g_fw_info[FW_TYPE].field_product_name_ptr,
			   MIN((size_t)g_fw_info[FW_TYPE].field_product_name_len,
				   sizeof(OD_PERSIST_COMM.x1008_manufacturerDeviceName) - 1U));
	}

	/// 0x1009 manufacturerHardwareVersion
	if(g_fw_info[FW_TYPE].field_product_ptr &&
	   g_fw_info[FW_TYPE].field_product_len > 0)
	{
		memcpy(OD_PERSIST_COMM.x1009_manufacturerHardwareVersion,
			   g_fw_info[FW_TYPE].field_product_ptr,
			   MIN((size_t)g_fw_info[FW_TYPE].field_product_len,
				   sizeof(OD_PERSIST_COMM.x1009_manufacturerHardwareVersion) - 1U));
	}

	/// 0x100A manufacturerSoftwareVersion
	int ptr = 0, sz = sizeof(OD_PERSIST_COMM.x100A_manufacturerSoftwareVersion);

	_print_num(OD_PERSIST_COMM.x100A_manufacturerSoftwareVersion, &ptr, sz, g_fw_info[FW_TYPE].ver_major);
	if(ptr < sz - 1) OD_PERSIST_COMM.x100A_manufacturerSoftwareVersion[ptr++] = '.';
	_print_num(OD_PERSIST_COMM.x100A_manufacturerSoftwareVersion, &ptr, sz, g_fw_info[FW_TYPE].ver_minor);
	if(ptr < sz - 1) OD_PERSIST_COMM.x100A_manufacturerSoftwareVersion[ptr++] = '.';
	_print_num(OD_PERSIST_COMM.x100A_manufacturerSoftwareVersion, &ptr, sz, g_fw_info[FW_TYPE].ver_patch);

	OD_PERSIST_COMM.x1018_identity.UID0 = g_uid[0];
	OD_PERSIST_COMM.x1018_identity.UID1 = g_uid[1];
	OD_PERSIST_COMM.x1018_identity.UID2 = g_uid[2];

	const char *p_build_ts = fw_fields_find_by_key_helper(&g_fw_info[FW_TYPE], "build_ts");
	if(p_build_ts)
	{
		memcpy(OD_PERSIST_COMM.x1018_identity.buildTimedate,
			   p_build_ts,
			   MIN((size_t)strlen(p_build_ts), sizeof(OD_PERSIST_COMM.x1018_identity.buildTimedate) - 1U));
	}
}

#if FW_TYPE == FW_APP
bool co_is_master_timeout(void) { return CO->HBcons->monitoredNodes[0].HBstate != CO_HBconsumer_ACTIVE; }
#endif