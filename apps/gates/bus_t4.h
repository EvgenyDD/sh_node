#ifndef BUS_T4_H__
#define BUS_T4_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
	PH_IDLE = 0,

	PH_CNT,
};

typedef struct
{
	uint32_t err_ovf, err_crc, err_pkt_short;
	uint32_t ok_rx, ok_tx;
} bus_t4_t;

void bus_t4_init(void);
void bus_t4_poll(uint32_t diff_ms);

void bus_t4_send(uint8_t *data, uint16_t length);

inline static void memcpy_volatile(volatile void *dst, const volatile void *src, size_t size)
{
	for(uint32_t i = 0; i < size; i++)
	{
		*((volatile uint8_t *)dst + i) = *((const volatile uint8_t *)src + i);
	}
}

extern bus_t4_t bus;

#endif // BUS_T4_H__