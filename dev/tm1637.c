#include "tm1637.h"
#include "CANopen.h"
#include "OD.h"
#include "cfg_device.h"
#include "platform.h"
#include <stdarg.h>
#include <stdio.h>

#ifdef CFG_USE_TM1637

#define CLK_Port GPIOB // B6 - CLK
#define CLK_Pin (1 << 6)
#define DATA_Port GPIOB // B7 - DATA
#define DATA_Pin (1 << 7)

static uint8_t seg_cnt = 4;

#define TM1637_COMM1 0x40
#define TM1637_COMM2 0xC0
#define TM1637_COMM3_OFF 0x80
#define TM1637_COMM3_ON 0x88
#define TM1637_SEG_MAX 6

static void tm1637_delay(void)
{
	for(volatile uint32_t i = 0; i < 6; i++)
	{
		asm("nop");
	}
}

static void _start(void)
{
	PIN_SET(CLK);
	PIN_SET(DATA);
	tm1637_delay();

	PIN_RST(DATA);
	tm1637_delay();
}

/**
 * @brief Generates a stop condition for the TM1637 communication.
 *        This function generates a stop condition by toggling the data and clock lines.
 *        The stop condition signals the end of communication with the TM1637 display.
 * @param[in] handle Pointer to the TM1637 handle structure.
 */
static void _stop(void)
{
	PIN_RST(DATA);
	tm1637_delay();

	PIN_SET(CLK);
	tm1637_delay();

	PIN_SET(DATA);
	tm1637_delay();
}

static int tm1637_write(uint8_t data)
{
	uint8_t tmp = data;

	/* Send each bit (LSB first) */
	for(int i = 0; i < 8; i++)
	{
		PIN_RST(CLK);
		tm1637_delay();

		PIN_WR(DATA, tmp & 0x01);
		tm1637_delay();
		PIN_SET(CLK);
		tm1637_delay();
		tmp >>= 1;
	}
	PIN_RST(CLK);
	PIN_SET(DATA);
	tm1637_delay();

	/* Generate clock pulse for ACK phase */
	PIN_SET(CLK);
	tm1637_delay();
	tm1637_delay();

	/* Read ACK bit from TM1637 — data line is released and clock is toggled to sample response */
	tmp = PIN_GET(DATA) ? 1 : 0;
	PIN_WR(DATA, tmp != 0);

	tm1637_delay();
	PIN_RST(CLK);
	tm1637_delay();
	return tmp;
}

int tm1637_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	PIN_SET(CLK);
	PIN_SET(DATA);

	delay_ms(1);

	_start();
	int err = tm1637_write(TM1637_COMM1);
	_stop();
	return err;
}

int tm1637_brightness(uint8_t brightness_0_8)
{
	uint8_t tmp = brightness_0_8 > 8 ? 8 : brightness_0_8;
	tmp = (brightness_0_8 == 0) ? TM1637_COMM3_OFF : TM1637_COMM3_ON;
	if(brightness_0_8 > 0)
	{
		brightness_0_8--;
	}

	_start();
	int err = tm1637_write(tmp | brightness_0_8);
	_stop();
	return err;
}

/**
 * @brief Displays raw segment data on the TM1637 display.
 * @param[in] handle Pointer to the TM1637 handle structure.
 * @param[in] data Pointer to an array containing raw segment data.
 */
int tm1637_raw(const uint8_t *data)
{
	_start();
	int err = tm1637_write(TM1637_COMM2);
	if(err) return err;

	for(uint8_t i = 0; i < seg_cnt; i++)
	{
		err = tm1637_write(data[i]);
		if(err) break;
	}
	_stop();
	return err;
}

/**
 * @brief Displays a string on the TM1637 7-segment display.
 * @param[in] handle Pointer to the TM1637 handle structure.
 * @param[in] str Pointer to a null-terminated string to display.
 *               Supports numbers (0-9), letters, '-' and '.' for decimal points.
 */
int tm1637_str(const char *str)
{
	uint8_t buff[TM1637_SEG_MAX + 1] = {0};
	const char *str_tmp = (const char *)str;

	for(int i = 0; i < seg_cnt; i++)
	{
		switch(*str_tmp)
		{
		case '0':
			buff[i] = 0x3f;
			break;
		case '1':
			buff[i] = 0x06;
			break;
		case '2':
			buff[i] = 0x5b;
			break;
		case '3':
			buff[i] = 0x4f;
			break;
		case '4':
			buff[i] = 0x66;
			break;
		case '5':
			buff[i] = 0x6d;
			break;
		case '6':
			buff[i] = 0x7d;
			break;
		case '7':
			buff[i] = 0x07;
			break;
		case '8':
			buff[i] = 0x7f;
			break;
		case '9':
			buff[i] = 0x6f;
			break;
		case '-':
			buff[i] = 0x40;
			break;
		case 'A':
		case 'a':
			buff[i] = 0x77;
			break;
		case 'B':
		case 'b':
			buff[i] = 0x7C;
			break;
		case 'C':
		case 'c':
			buff[i] = 0x58;
			break;
		case 'D':
		case 'd':
			buff[i] = 0x5E;
			break;
		case 'E':
		case 'e':
			buff[i] = 0x79;
			break;
		case 'F':
		case 'f':
			buff[i] = 0x71;
			break;
		case 'G':
		case 'g':
			buff[i] = 0x6f;
			break;
		case 'H':
		case 'h':
			buff[i] = 0x76;
			break;
		case 'I':
		case 'i':
			buff[i] = 0x04;
			break;
		case 'J':
		case 'j':
			buff[i] = 0x0E;
			break;
		case 'L':
		case 'l':
			buff[i] = 0x38;
			break;
		case 'N':
		case 'n':
			buff[i] = 0x54;
			break;
		case 'O':
		case 'o':
			buff[i] = 0x5C;
			break;
		case 'P':
		case 'p':
			buff[i] = 0x73;
			break;
		case 'Q':
		case 'q':
			buff[i] = 0x67;
			break;
		case 'R':
		case 'r':
			buff[i] = 0x50;
			break;
		case 'S':
		case 's':
			buff[i] = 0x6D;
			break;
		case 'T':
		case 't':
			buff[i] = 0x78;
			break;
		case 'U':
		case 'u':
			buff[i] = 0x1C;
			break;
		case 'Y':
		case 'y':
			buff[i] = 0x6E;
			break;
		case '.':
			buff[i] = 0x80;
			break;
		default:
			buff[i] = 0;
			break;
		}
		// if(*(str_tmp + 1) == '.')
		// {
		// 	buff[i] |= 0x80;
		// 	str_tmp++;
		// }
		str_tmp++;
	}

	return tm1637_raw(buff);
}

/**
 * @brief Displays a formatted string on the TM1637 7-segment display.
 * @param[in] handle Pointer to the TM1637 handle structure.
 * @param[in] format Format string (printf-style) to display.
 * @param[in] ... Additional arguments for the formatted string.
 */
int tm1637_printf(const char *format, ...)
{
	char buff[TM1637_SEG_MAX + 1] = {0};

	va_list args;
	va_start(args, format);
	int chars_written = vsnprintf(buff, sizeof(buff), format, args);
	va_end(args);
	if(chars_written < 0) return -1;

	return tm1637_str(buff);
}

int tm1637_clear(void)
{
	const uint8_t buff[TM1637_SEG_MAX] = {0};
	return tm1637_raw(buff);
}

#endif