#include "eeprom.h"
#include "i2c_common.h"

#define EEPROM_ADDR 0x50
#define PAGE_SIZE 16

static int eeprom_wait_write_finish(void)
{
	int sts = !0;
	for(int cnt = 0; sts != 0 && cnt < 32; cnt++)
	{
		sts = i2c_common_check_addr(EEPROM_ADDR);
	}
	return sts;
}

int eeprom_read(uint32_t addr, uint8_t *data, uint32_t size)
{
	if(addr + size > EEPROM_SIZE) return EEP_ERR_ADDR_OVF;
	return i2c_common_write_to_read(EEPROM_ADDR | ((addr >> 8) & 0x3), addr & 0xFF, data, size);
}

static int eeprom_write_page(uint32_t addr, const uint8_t *data, uint32_t size)
{
	if(addr + size > EEPROM_SIZE) return EEP_ERR_ADDR_OVF;

	int sts = i2c_common_write(EEPROM_ADDR | ((addr >> 8) & 0x3), addr & 0xFF, data, size);
	if(sts) return sts;
	return eeprom_wait_write_finish();
}

int eeprom_write(uint32_t addr, const uint8_t *data, uint32_t size)
{
	if(addr + size > EEPROM_SIZE) return EEP_ERR_ADDR_OVF;

	uint32_t p_data = 0;
	for(;;)
	{
		uint32_t remain_to_page_end = PAGE_SIZE - (addr % PAGE_SIZE);
		uint32_t wr = remain_to_page_end > size ? size : remain_to_page_end;
		int sts = eeprom_write_page(addr, &data[p_data], wr);
		if(sts) return sts;
		addr += wr;
		p_data += wr;
		size -= wr;
		if(size == 0) return sts;
	}
}
