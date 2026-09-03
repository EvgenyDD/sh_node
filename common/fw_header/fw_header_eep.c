#include "eeprom.h"
#include "fw_header.h"
#include "i2c_common.h"
#include "platform.h"

#ifdef FW_HEADER_CANOPEN
#include "CANopen.h"
#include "OD.h"

static ODR_t fw_hdr_eep_wr(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *countWritten)
{
	if(stream == NULL || buf == NULL || countWritten == NULL) return ODR_DEV_INCOMPAT;

	OD_size_t endOffset = stream->dataOffset + count;

	if(endOffset > STR_DEV_LEN) return ODR_DATA_LONG;

	uint16_t currentEEPROMAddr = STR_DEV_ADDR + stream->dataOffset;

	if(eeprom_write(currentEEPROMAddr, (const uint8_t *)buf, count)) return ODR_GENERAL;

	*countWritten = count;

	if(endOffset < STR_DEV_LEN) return ODR_OK;

	return ODR_OK;
}

static ODR_t fw_hdr_eep_rd(OD_stream_t *stream, void *buf, OD_size_t count, OD_size_t *countRead)
{
	if(stream == NULL || buf == NULL || countRead == NULL) return ODR_DEV_INCOMPAT;

	OD_size_t dataLenToCopy = STR_DEV_LEN;
	const uint8_t *dataOrig = (const uint8_t *)g_p_str_dev_name;

	if(dataOrig == NULL) return ODR_SUB_NOT_EXIST;

	ODR_t returnCode = ODR_OK;

	if(stream->dataOffset == 0)
	{
		if(eeprom_read(STR_DEV_ADDR, (uint8_t *)g_p_str_dev_name, STR_DEV_LEN)) return ODR_GENERAL;
	}

	if(stream->dataOffset > 0 || dataLenToCopy > count)
	{
		if(stream->dataOffset >= dataLenToCopy) return ODR_DEV_INCOMPAT;

		dataLenToCopy -= stream->dataOffset;
		dataOrig += stream->dataOffset;

		if(dataLenToCopy > count)
		{
			dataLenToCopy = count;
			stream->dataOffset += dataLenToCopy;
			returnCode = ODR_PARTIAL;
		}
		else
		{
			stream->dataOffset = 0; /* copy finished, reset offset */
		}
	}

	memcpy(buf, dataOrig, dataLenToCopy);
	*countRead = dataLenToCopy;
	return returnCode;
}

static OD_extension_t ext = {.object = NULL, .read = fw_hdr_eep_rd, .write = fw_hdr_eep_wr};

void fw_header_eep_od_init(void)
{
	i2c_init();
	eeprom_read(STR_DEV_ADDR, (uint8_t *)g_p_str_dev_name, STR_DEV_LEN);

	OD_extension_init(OD_ENTRY_H1F58_prod_id, &ext);
}
#endif // FW_HEADER_CANOPEN