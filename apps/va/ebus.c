#include "ebus.h"
#include "CANopen.h"
#include "OD.h"
#include "_printf.h"
#include "platform.h"
#include "stm32f10x.h"
#include <stdbool.h>

#define DBG

extern volatile uint8_t trig;

static volatile uint32_t pi = 0; // tmp

static volatile uint8_t ph = PH_IDLE, ph_pend = PH_IDLE;
static uint32_t ph_tmr[PH_CNT] = {0}; // timers for phase timeouts
static const uint32_t ph_to[PH_CNT] = {
	0,	 // PH_IDLE
	50,	 // PH_WAIT_SYN_TO_TX
	0,	 // PH_TX
	20,	 // PH_WAIT_RESP
	20,	 // PH_RX_SYN
	30,	 // PH_RX_HDR
	120, // PH_RX_PL
	20,	 // PH_RX_CRC
	0,	 // PH_TX_RESP
};

#define SYN 0xAA	   // synchronization
#define ESC 0xA9	   // escape symbol, either followed by 0x00 for the value 0xA9, or 0x01 for the value 0xAA
#define ACK 0x00	   // positive acknowledge
#define NACK 0xFF	   // negative acknowledge
#define BROADCAST 0xFE // broadcast destination addr

#define QQ 0x31 // src
#define ZZ 0x08 // dst - boiler
#define PB_VAILLANT 0xB5
#define SB_03 0x03
#define SB_09 0x09
#define SB_10 0x10
#define SB_11 0x11

#define CMD_STATUS01 1
#define CMD_STATUS02 2

volatile boiler_t boiler = {0};

static volatile uint8_t rx_buf[256] = {0};
static volatile uint32_t rx_buf_ptr = 0, pkt_pend_size = 0;

static volatile uint8_t tx_buf[256] = {0}, tx_buf_resp[2] = {ACK, SYN}, tx_buf_resp_cmd[3] = {0, 0, 0};

static volatile uint8_t ebus_tx_busy = 0;

#define MAX_SLAVE_DATA_LEN 25
#define MAX_MASTER_CMD_LEN 32
#define MAX_RX_BUF_SIZE (MAX_MASTER_CMD_LEN + 2)

volatile struct
{
	bool is_escaped;
	uint8_t data_len;		// Expected payload size from the LEN byte
	uint8_t bytes_received; // Counter for unescaped data bytes
	uint8_t crc_byte;
	uint8_t buffer[MAX_RX_BUF_SIZE]; // Holds [LEN] + [DATA_BYTES...]
	uint8_t buf_idx;
	bool is_master_cmd;
} parser = {0};

static void ebus_parser_init(void)
{
	parser.is_escaped = false;
	parser.data_len = 0;
	parser.bytes_received = 0;
	parser.buf_idx = 0;
	parser.crc_byte = 0;
	parser.is_master_cmd = false;
}

static void uart_tx_dma(const volatile uint8_t *data, uint32_t len)
{
	if(len == 0) return;

	delay_ms(1);
	while(ebus_tx_busy)
		;
	while(DMA_GetCurrDataCounter(DMA1_Channel4) > 0)
		;

	USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
	ebus_tx_busy = 1;

	DMA_Cmd(DMA1_Channel4, DISABLE);
	DMA1_Channel4->CMAR = (uint32_t)data;
	DMA1_Channel4->CNDTR = len;
	DMA_ClearFlag(DMA1_FLAG_TC4 | DMA1_FLAG_HT4 | DMA1_FLAG_TE4 | DMA1_FLAG_GL4);
	DMA_Cmd(DMA1_Channel4, ENABLE);
}

void ebus_init(void)
{
#ifdef DBG
	{
		GPIO_InitTypeDef GPIO_InitStruct = {0};
		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(GPIOD, &GPIO_InitStruct);

		RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

		GPIO_InitTypeDef GPIO_InitStructure;
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
		GPIO_Init(GPIOA, &GPIO_InitStructure);

		USART_InitTypeDef USART_InitStructure;
		USART_InitStructure.USART_BaudRate = 460800;
		USART_InitStructure.USART_WordLength = USART_WordLength_8b;
		USART_InitStructure.USART_StopBits = USART_StopBits_1;
		USART_InitStructure.USART_Parity = USART_Parity_No;
		USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
		USART_InitStructure.USART_Mode = USART_Mode_Tx; // Enable TX only

		USART_Init(USART2, &USART_InitStructure);
		USART_Cmd(USART2, ENABLE);
	}
#endif

	// B6 tx B7 rx
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_PinRemapConfig(GPIO_Remap_USART1, ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	DMA_InitTypeDef DMA_InitStructure = {0};
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(USART1->DR);
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)0;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
	DMA_InitStructure.DMA_BufferSize = 0;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel4, &DMA_InitStructure);

	DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, ENABLE);

	NVIC_InitTypeDef NVIC_InitStructure = {0};
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_InitTypeDef USART_InitStructure = {0};
	USART_InitStructure.USART_BaudRate = 2400;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_Cmd(USART1, ENABLE);
}

#ifdef DBG
static void _send_dbg(const volatile uint8_t *data, uint32_t length)
{
	for(uint32_t i = 0; i < length; i++)
	{
		while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET)
			;
		USART_SendData(USART2, data[i]);
	}
}
#endif

static const uint8_t CRC_LOOKUP_TABLE[] = {
	0x00, 0x9b, 0xad, 0x36, 0xc1, 0x5a, 0x6c, 0xf7, 0x19, 0x82, 0xb4, 0x2f, 0xd8, 0x43, 0x75, 0xee,
	0x32, 0xa9, 0x9f, 0x04, 0xf3, 0x68, 0x5e, 0xc5, 0x2b, 0xb0, 0x86, 0x1d, 0xea, 0x71, 0x47, 0xdc,
	0x64, 0xff, 0xc9, 0x52, 0xa5, 0x3e, 0x08, 0x93, 0x7d, 0xe6, 0xd0, 0x4b, 0xbc, 0x27, 0x11, 0x8a,
	0x56, 0xcd, 0xfb, 0x60, 0x97, 0x0c, 0x3a, 0xa1, 0x4f, 0xd4, 0xe2, 0x79, 0x8e, 0x15, 0x23, 0xb8,
	0xc8, 0x53, 0x65, 0xfe, 0x09, 0x92, 0xa4, 0x3f, 0xd1, 0x4a, 0x7c, 0xe7, 0x10, 0x8b, 0xbd, 0x26,
	0xfa, 0x61, 0x57, 0xcc, 0x3b, 0xa0, 0x96, 0x0d, 0xe3, 0x78, 0x4e, 0xd5, 0x22, 0xb9, 0x8f, 0x14,
	0xac, 0x37, 0x01, 0x9a, 0x6d, 0xf6, 0xc0, 0x5b, 0xb5, 0x2e, 0x18, 0x83, 0x74, 0xef, 0xd9, 0x42,
	0x9e, 0x05, 0x33, 0xa8, 0x5f, 0xc4, 0xf2, 0x69, 0x87, 0x1c, 0x2a, 0xb1, 0x46, 0xdd, 0xeb, 0x70,
	0x0b, 0x90, 0xa6, 0x3d, 0xca, 0x51, 0x67, 0xfc, 0x12, 0x89, 0xbf, 0x24, 0xd3, 0x48, 0x7e, 0xe5,
	0x39, 0xa2, 0x94, 0x0f, 0xf8, 0x63, 0x55, 0xce, 0x20, 0xbb, 0x8d, 0x16, 0xe1, 0x7a, 0x4c, 0xd7,
	0x6f, 0xf4, 0xc2, 0x59, 0xae, 0x35, 0x03, 0x98, 0x76, 0xed, 0xdb, 0x40, 0xb7, 0x2c, 0x1a, 0x81,
	0x5d, 0xc6, 0xf0, 0x6b, 0x9c, 0x07, 0x31, 0xaa, 0x44, 0xdf, 0xe9, 0x72, 0x85, 0x1e, 0x28, 0xb3,
	0xc3, 0x58, 0x6e, 0xf5, 0x02, 0x99, 0xaf, 0x34, 0xda, 0x41, 0x77, 0xec, 0x1b, 0x80, 0xb6, 0x2d,
	0xf1, 0x6a, 0x5c, 0xc7, 0x30, 0xab, 0x9d, 0x06, 0xe8, 0x73, 0x45, 0xde, 0x29, 0xb2, 0x84, 0x1f,
	0xa7, 0x3c, 0x0a, 0x91, 0x66, 0xfd, 0xcb, 0x50, 0xbe, 0x25, 0x13, 0x88, 0x7f, 0xe4, 0xd2, 0x49,
	0x95, 0x0e, 0x38, 0xa3, 0x54, 0xcf, 0xf9, 0x62, 0x8c, 0x17, 0x21, 0xba, 0x4d, 0xd6, 0xe0, 0x7b};

// static void updateCrc(uint8_t value, uint8_t *crc) { *crc = CRC_LOOKUP_TABLE[*crc] ^ value; }

// static uint8_t ebus_crc(const volatile uint8_t *data, uint32_t length)
// {
// 	uint8_t crc = 0;
// 	for(uint32_t i = 0; i < length; i++)
// 	{
// 		uint8_t value = data[i];
// 		if(value == ESC)
// 		{
// 			updateCrc(ESC, &crc);
// 			updateCrc(0x00, &crc);
// 		}
// 		else if(value == SYN)
// 		{
// 			updateCrc(ESC, &crc);
// 			updateCrc(0x01, &crc);
// 		}
// 		else
// 		{
// 			updateCrc(value, &crc);
// 		}
// 	}
// 	return crc;
// }

static void switch_ph_now(uint8_t new_ph)
{
	ph = ph_pend = new_ph;
	ph_tmr[ph] = ph_to[ph];
}

int ebus_send(const uint8_t *data, uint32_t len)
{
	uint8_t crc = 0;
	uint32_t p = 0;

	for(uint32_t i = 0; i < len; i++)
	{
		uint8_t val = data[i];
		if(val == ESC)
		{
			if(p + 2 > sizeof(tx_buf)) return -1;
			crc = CRC_LOOKUP_TABLE[crc] ^ ESC;
			crc = CRC_LOOKUP_TABLE[crc] ^ 0x0;
			tx_buf[p++] = ESC;
			tx_buf[p++] = 0x0;
		}
		else if(val == SYN)
		{
			if(p + 2 > sizeof(tx_buf)) return -1;
			crc = CRC_LOOKUP_TABLE[crc] ^ ESC;
			crc = CRC_LOOKUP_TABLE[crc] ^ 0x1;
			tx_buf[p++] = ESC;
			tx_buf[p++] = 0x01;
		}
		else
		{
			if(p + 1 > sizeof(tx_buf)) return -1;
			crc = CRC_LOOKUP_TABLE[crc] ^ val;
			tx_buf[p++] = val;
		}
	}

	if(crc == ESC)
	{
		if(p + 2 > sizeof(tx_buf)) return -1;
		tx_buf[p++] = ESC;
		tx_buf[p++] = 0x00;
	}
	else if(crc == SYN)
	{
		if(p + 2 > sizeof(tx_buf)) return -1;
		tx_buf[p++] = ESC;
		tx_buf[p++] = 0x01;
	}
	else
	{
		if(p + 1 > sizeof(tx_buf)) return -1;
		tx_buf[p++] = crc;
	}

	pkt_pend_size = p;
	return 0;
}

#ifdef DBG
static char print_buf[256];

static void _PRINTF(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int act_len = vsnprintf(print_buf, sizeof(print_buf) - 1, fmt, ap);
	va_end(ap);

	_send_dbg((uint8_t *)print_buf, act_len);
}
#endif

static uint8_t *packets[TST_PKT_COUNT] = {
	(uint8_t[]){QQ, ZZ, PB_VAILLANT, SB_11, 1, CMD_STATUS01},
	(uint8_t[]){QQ, ZZ, PB_VAILLANT, SB_11, 1, CMD_STATUS02},
	(uint8_t[]){QQ, ZZ, PB_VAILLANT, SB_09, 3, 0x0d, 0x39, 0},	  // flowtempdesired
	(uint8_t[]){QQ, ZZ, PB_VAILLANT, SB_03, 2, 0x00, 0x01},		  // currenterror
	(uint8_t[]){QQ, ZZ, PB_VAILLANT, SB_09, 3, 0x0d, 0xAB, 0x00}, // Statenumber
	(uint8_t[]){QQ, ZZ, PB_VAILLANT, SB_10, 9 /* len*/,
				0,
				0,	  // hcmode: 0-auto, 1-off, 2-heat, 3-water
				0x8c, // flowtempdesired: 70.0 *2
				0x64, // hwctempdesired: 50.0 *2
				0x3c, // hwcflowtempdesired: 60.0
				0,
				0, // disablehc - 0x1, disablehwcload - 0x04
				0, 0},
};

void ebus_poll(uint32_t diff_ms)
{
	if(ph != ph_pend)
	{
		ph = ph_pend;
		ph_tmr[ph] = ph_to[ph];
	}

	if(ph <= PH_CNT)
	{
		if(ph_tmr[ph] > 0)
		{
			ph_tmr[ph] = ph_tmr[ph] > diff_ms ? ph_tmr[ph] - diff_ms : 0;
			if(ph_tmr[ph] == 0)
			{
#ifdef DBG
				_PRINTF("to %d", ph);
#endif
				boiler.err[ph]++;
				switch_ph_now(PH_IDLE);
			}
		}
	}

	if(trig)
	{

		if(ph == PH_IDLE)
		{
			if(++pi >= TST_PKT_COUNT) pi = 0;

#ifdef DBG
			_PRINTF("send pkt %d", pi);
#endif

			if(ebus_send(packets[pi], packets[pi][4] + 5) == 0)
			{
				switch_ph_now(PH_WAIT_SYN_TO_TX);
				trig = 0;
			}
		}
	}
}

static int check_unstaffing(uint8_t *raw_byte)
{
	if(!parser.is_escaped && *raw_byte == ESC)
	{
		parser.is_escaped = true;
		return 1;
	}

	if(parser.is_escaped)
	{
		parser.is_escaped = false;
		if(*raw_byte == 0x00)
		{
			*raw_byte = ESC;
		}
		else if(*raw_byte == 0x01)
		{
			*raw_byte = SYN;
		}
		else
		{
			ebus_parser_init();
			return -1;
		}
	}
	return 0;
}

static void parse_pkt(const volatile uint8_t *pkt, uint32_t len)
{
#ifdef DBG
	_PRINTF("R");
	_send_dbg(pkt, len);
#endif

	if(pi < TST_PKT_COUNT)
	{
		for(uint32_t i = 0; i < len; i++)
			boiler.rx_pkt[pi][i] = pkt[i];
		boiler.rx_pkt_len[pi] = len;
	}
}

static void parse_cmd_pkt(const volatile uint8_t *pkt, uint32_t len)
{
#ifdef DBG
	_PRINTF("C");
	_send_dbg(pkt, len);
#endif
}

static void parse(uint8_t ch)
{
	int sts;
	switch(ph)
	{
	case PH_TX:
	case PH_TX_RESP: break;

	case PH_WAIT_SYN_TO_TX:
		if(ch == SYN) // wait master sync, now transmit packet
		{
			ph_pend = PH_TX;
#ifdef DBG
			_PRINTF("W%ld", pkt_pend_size);
#endif
			uart_tx_dma(tx_buf, pkt_pend_size);
		}
		break;

	case PH_WAIT_RESP: // wait response after packet send
		if(ch == ACK)
		{
			ebus_parser_init();
			ph_pend = PH_RX_SYN;
			boiler.ok_ack++;
		}
		else if(ch == NACK)
		{
			boiler.err_nak++;
			ph_pend = PH_IDLE;
		}
		else
		{
			boiler.err_unknw_resp++;
			ph_pend = PH_IDLE;
		}
		break;

	case PH_IDLE:
	case PH_RX_SYN:
		// First, we check the "naked" SYN, since it cannot be escaped. If there is a SYN (0xAA) stream, we simply reset it and prepare for a new command.
		if(ch == SYN)
		{
			parser.is_master_cmd = true;
			parser.buf_idx = 0;
			parser.bytes_received = 0;
			parser.is_escaped = false;
			ph_pend = PH_RX_SYN;
			break;
		}
		// For all other bytes we apply destuffing
		sts = check_unstaffing(&ch);
		if(sts < 0) ph_pend = PH_IDLE;
		if(sts) break;

		if(parser.is_master_cmd)
		{
			// The previous byte was SYN and the current one is NOT SYN (this is the source address of the QQ master)
			parser.buffer[parser.buf_idx++] = ch;
			ph_pend = PH_RX_HDR;
		}
		else if(ch <= MAX_SLAVE_DATA_LEN)
		{
			// If we are not in master mode and the correct length is received, this is the Slave response.
			parser.data_len = ch;
			parser.buffer[parser.buf_idx++] = ch;
			ph_pend = parser.data_len == 0 ? PH_RX_CRC : PH_RX_PL;
		}
		else
		{
			boiler.err_big_sz++;
			ebus_parser_init();
			ph_pend = PH_IDLE;
		}
		break;

	case PH_RX_HDR:
		// We continue to assemble the team header (we already have QQ, we are adding ZZ, PB, SB, NN)
		parser.buffer[parser.buf_idx++] = ch;
		if(parser.buf_idx == 5) // Indexes: 0=QQ, 1=ZZ, 2=PB, 3=SB, 4=NN (length)
		{
			parser.data_len = ch; // NN bytes of command data
			if(parser.data_len == 0)
			{
				ph_pend = PH_RX_CRC;
			}
			else if((parser.buf_idx + parser.data_len) < MAX_RX_BUF_SIZE)
			{
				ph_pend = PH_RX_PL;
			}
			else
			{
				ebus_parser_init();
			}
		}
		break;

	case PH_RX_PL:
		sts = check_unstaffing(&ch);
		if(sts < 0) ph_pend = PH_IDLE;
		if(sts) break;

		parser.buffer[parser.buf_idx++] = ch;
		parser.bytes_received++;

		if(parser.bytes_received >= parser.data_len) ph_pend = PH_RX_CRC;
		break;

	case PH_RX_CRC:
		sts = check_unstaffing(&ch);
		if(sts < 0) ph_pend = PH_IDLE;
		if(sts) break;

		parser.crc_byte = ch;

		// If we have finished receiving or the boiler sends a SYN/bus release byte, we simply turn off the machine so as not to run a false CRC calculation through the buffer.
		if(ch == SYN)
		{
			ph_pend = PH_IDLE;
			ebus_parser_init();
			break;
		}

		uint8_t calculated_crc = 0;
		for(uint32_t i = 0; i < parser.buf_idx; i++)
		{
			calculated_crc = CRC_LOOKUP_TABLE[calculated_crc] ^ parser.buffer[i];
		}

		if(calculated_crc == parser.crc_byte)
		{
			parser.is_master_cmd ? parse_cmd_pkt(parser.buffer, parser.buf_idx)
								 : parse_pkt(&parser.buffer[1], parser.data_len);
			tx_buf_resp[0] = ACK;
			parser.is_master_cmd ? boiler.ok_cmd_pkt_rx++ : boiler.ok_pkt_rx++;
		}
		else
		{
			tx_buf_resp[0] = NACK;
			boiler.err_crc++;
		}

		if(parser.is_master_cmd == false)
		{
			ph_pend = PH_TX_RESP;
#ifdef DBG
			_PRINTF("E%d", 2);
#endif
			uart_tx_dma(tx_buf_resp, 2);
			ebus_parser_init();
			parser.is_master_cmd = true; // beсause last transmitted was 0xAA
		}
		else if(parser.is_master_cmd)
		{
			ph_pend = PH_TX_RESP;
#ifdef DBG
			_PRINTF("Y%d", 3);
#endif
			uart_tx_dma(tx_buf_resp_cmd, 3);
			ebus_parser_init();
		}
		break;

	default: ph_pend = PH_IDLE; break;
	}
}

void USART1_IRQHandler(void)
{
	if(USART_GetITStatus(USART1, USART_IT_RXNE))
	{
		volatile uint8_t ch = USART_ReceiveData(USART1) & 0xFF;
		parse(ch);
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
	NVIC_ClearPendingIRQ(USART1_IRQn);
}

void DMA1_Channel4_IRQHandler(void)
{
	if(DMA_GetITStatus(DMA1_IT_TC4) != RESET)
	{
		DMA_ClearITPendingBit(DMA1_IT_TC4);

		while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
			;

		volatile uint8_t dummy = USART_ReceiveData(USART1); // Clearing the hardware echo that is stuck in the DR
		(void)dummy;
		USART_ClearFlag(USART1, USART_FLAG_RXNE);

		ebus_tx_busy = 0;
		if(ph_pend == PH_TX) ph_pend = PH_WAIT_RESP;
		if(ph_pend == PH_TX_RESP) ph_pend = PH_IDLE;

		USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	}
}