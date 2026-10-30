#ifndef GPS_H__
#define GPS_H__

#include <stdbool.h>
#include <stdint.h>

enum
{
	PH_IDLE = 0,
	PH_WAIT_SYN_TO_TX,
	PH_TX_DELAY,
	PH_TX_DELAY_RESP,
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
	uint32_t err_ovf, err_nak, err_unknw_resp, err_big_sz, err_crc, err_sent_setpoint;
	uint32_t ok_ack, ok_pkt_rx, ok_cmd_pkt_rx;
} boiler_t;

void ebus_init(void);
void ebus_poll(uint32_t diff_ms);
void ebus_enable(bool state);
int ebus_send(const uint8_t *data, uint32_t len);

void ebus_set_flow_temp(uint8_t flow_temp);
void ebus_set_hwc_temp(uint8_t hwc_temp);
void ebus_set_heat_mode(uint8_t hc_mode, bool disable_hc, bool disable_hwc);

extern volatile boiler_t boiler;

#endif // GPS_H__