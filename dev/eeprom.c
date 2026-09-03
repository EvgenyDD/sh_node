#include "eeprom.h"
#include "i2c_common.h"
#include "platform.h"

#define EEP_PAGE_SIZE 16

static inline void delay_100us_loop(void)
{
	// 6400 total cycles needed / 4 cycles per loop iteration = 1600 iterations
	uint32_t count = 1600;

	__asm volatile(
		"1: subs %0, #1 \n\t" // 1 cycle: Subtract 1 from count
		"bne 1b         \n\t" // 3 cycles if branch taken, 1 cycle if not
		: "+r"(count)
		:
		: "cc");
}

static int eeprom_wait_write_finish(void)
{
	int sts = !0;
	for(int cnt = 0; sts != 0 && cnt < 100; cnt++)
	{
		sts = i2c_is_device_ready(EEPROM_ADDR << 1, 1);
		delay_100us_loop();
	}
	return sts;
}

int eeprom_read(uint32_t addr, uint8_t *data, uint32_t size)
{
	if(addr + size > EEPROM_SIZE) return EEP_ERR_ADDR_OVF;
	return i2c_mem_read((EEPROM_ADDR | ((addr >> 8) & 0x3)) << 1, addr & 0xFF, false, data, size);
}

static int eeprom_write_page(uint32_t addr, const uint8_t *data, uint32_t size)
{
	if(addr + size > EEPROM_SIZE) return EEP_ERR_ADDR_OVF;

	int sts = i2c_mem_write((EEPROM_ADDR | ((addr >> 8) & 0x3)) << 1, addr & 0xFF, false, data, size);
	if(sts) return sts;
	return eeprom_wait_write_finish();
}

int eeprom_write(uint32_t addr, const uint8_t *data, uint32_t size)
{
	if(addr + size > EEPROM_SIZE) return EEP_ERR_ADDR_OVF;

	uint32_t p_data = 0;
	for(;;)
	{
		uint32_t remain_to_page_end = EEP_PAGE_SIZE - (addr % EEP_PAGE_SIZE);
		uint32_t wr = remain_to_page_end > size ? size : remain_to_page_end;
		int sts = eeprom_write_page(addr, &data[p_data], wr);
		if(sts) return sts;
		addr += wr;
		p_data += wr;
		size -= wr;
		if(size == 0) return sts;
	}
}
