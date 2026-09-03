#ifndef GPS_H__
#define GPS_H__

#include <stdbool.h>
#include <stdint.h>

#define TST_PKT_COUNT 6

enum
{
	PH_IDLE = 0,
	PH_WAIT_SYN_TO_TX,
	PH_TX,
	PH_WAIT_RESP, // ack/nack
	PH_RX_SYN,
	PH_RX_HDR,
	PH_RX_PL,
	PH_RX_CRC,
	PH_TX_RESP,

	PH_CNT,
};

typedef struct
{
	uint32_t err[PH_CNT];
	uint32_t ok_ack, err_nak, err_unknw_resp, err_big_sz, ok_pkt_rx, ok_cmd_pkt_rx, err_crc;

	uint8_t rx_pkt[TST_PKT_COUNT][32];
	uint32_t rx_pkt_len[TST_PKT_COUNT];
} boiler_t;

void ebus_init(void);
void ebus_poll(uint32_t diff_ms);

int ebus_send(const uint8_t *data, uint32_t len);

extern volatile boiler_t boiler;

#endif // GPS_H__