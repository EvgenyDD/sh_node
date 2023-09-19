#include "mag.h"
#include "i2c_common.h"
#include "platform.h"

// HMC5883L

#define POLL_INTERVAL 200

#define HMC5883L_ADDR 0x1E

#define HMC5883L_CONFREGA 0
#define HMC5883L_CONFREGB 1
#define HMC5883L_MODEREG 2
#define HMC5883L_DATAREGBEGIN 3
#define HMC5883L_REG_STATUS 9
#define HMC5883L_REG_IDA 10
#define HMC5883L_REG_IDB 11
#define HMC5883L_REG_IDC 12

//////////////////////

#define HMC5883L_MEASURECONTINOUS 0x00
#define HMC5883L_MEASURESINGLESHOT 0x01
#define HMC5883L_MEASUREIDLE 0x03
#define HMC5883L_MEASUREMODE HMC5883L_MEASURECONTINOUS

#define HMC5883L_NUM_SAMPLES1 (0 << 5)
#define HMC5883L_NUM_SAMPLES2 (1 << 5)
#define HMC5883L_NUM_SAMPLES4 (2 << 5)
#define HMC5883L_NUM_SAMPLES8 (3 << 5)

#define HMC5883L_RATE0_75 (0 << 2)
#define HMC5883L_RATE1_5 (1 << 2)
#define HMC5883L_RATE3_0 (2 << 2)
#define HMC5883L_RATE7_5 (3 << 2)
#define HMC5883L_RATE15 (4 << 2)
#define HMC5883L_RATE30 (5 << 2)
#define HMC5883L_RATE75 (6 << 2)

// setup scale
#define HMC5883L_SCALE088 0 // +-0.88 Gauss
#define HMC5883L_SCALE13 1	// +-1.3 Gauss
#define HMC5883L_SCALE19 2	// +-1.9 Gauss
#define HMC5883L_SCALE25 3	// +-2.5 Gauss
#define HMC5883L_SCALE40 4	// +-4.0 Gauss
#define HMC5883L_SCALE47 5	// +-4.7 Gauss
#define HMC5883L_SCALE56 6	// +-5.6 Gauss
#define HMC5883L_SCALE81 7	// +-8.1 Gauss
#define HMC5883L_SCALE HMC5883L_SCALE88

extern void delay_ms(volatile uint32_t delay_ms);

mag_t mag_data = {0};

static uint32_t tmr_poll = 0;

int mag_is_present(void)
{
	uint8_t mag_status[3];
	for(uint32_t i = 0; i < 3; i++)
	{
		if(i2c_common_write_to_read(HMC5883L_ADDR, HMC5883L_REG_IDA + i, &mag_status[i], 1) != 0) return false;
	}
	return mag_status[0] == 'H' && mag_status[1] == '4' && mag_status[2] == '3';
}

int mag_init(void)
{
	bool sensor_present = mag_is_present();

	if(sensor_present == false) return 1;

	uint8_t buf[2];
	if(i2c_common_write(HMC5883L_ADDR, HMC5883L_CONFREGA, (uint8_t[]){HMC5883L_NUM_SAMPLES4 | HMC5883L_RATE30}, 1) != 0) return 1;
	if(i2c_common_write(HMC5883L_ADDR, HMC5883L_CONFREGB, (uint8_t[]){HMC5883L_SCALE088 << 5}, 1) != 0) return 1;
	if(i2c_common_write(HMC5883L_ADDR, HMC5883L_MODEREG, (uint8_t[]){HMC5883L_MEASUREMODE}, 1) != 0) return 1;

	mag_data.sensor_present = true;
	return 0;
}

void mag_poll(uint32_t diff_ms)
{
	tmr_poll += diff_ms;
	if(tmr_poll >= POLL_INTERVAL)
	{
		tmr_poll = 0;

		uint8_t buf[6];
		int sts = i2c_common_write_to_read(HMC5883L_ADDR, HMC5883L_DATAREGBEGIN, buf, 6);
		if(sts == 0)
		{
			mag_data.mag_field[0] = (int16_t)((buf[0] << 8) | buf[1]);
			mag_data.mag_field[1] = (int16_t)((buf[2] << 8) | buf[3]);
			mag_data.mag_field[2] = (int16_t)((buf[4] << 8) | buf[5]);
		}
	}
}