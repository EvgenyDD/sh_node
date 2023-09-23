#include "gps.h"
#include "CANopen.h"
#include "OD.h"
#include "platform.h"
#include "uart_common.h"

// UBLOX NEO-M8N

static volatile uint8_t m8n_rx_buf[100];
static volatile uint32_t rx_cnt = 0;
static volatile bool m8n_rx_cplt_flag = false;

static M8N_UBX_NAV_PVT pvt;

static const unsigned char UBX_CFG_PRT[] = {
	0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00,
	0xD0, 0x08, 0x00, 0x00, 0x80, 0x25, 0x00, 0x00, 0x01, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9A, 0x79}; // UBX Protocol In, Out, UART1, 8N1-9600

static const unsigned char UBX_CFG_MSG[] = {
	0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0x01, 0x02, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x13, 0xBE}; // NAV-POSLLH(01-02), UART1

static const unsigned char UBX_CFG_RATE[] = {
	0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xC8, 0x00, 0x01, 0x00,
	0x01, 0x00, 0xDE, 0x6A}; // GPS Time, 5Hz Navigation Frequency

static const unsigned char UBX_CFG_CFG[] = {
	0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x31,
	0xBF}; // Save current configuration, Devices: BBR, FLASH, I2C-EEPROM, SPI-FLASH,

static const unsigned char UBX_CFG_MSGPVT[] = {
	0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0x01, 0x07, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x18, 0xE1}; // NAV-POSLLH(01-07), UART1

void gps_init(void)
{
	uart_tx(&UBX_CFG_PRT[0], sizeof(UBX_CFG_PRT));
	delay_ms(100);
	//	uart_tx(&UBX_CFG_MSG[0], sizeof(UBX_CFG_MSG));
	uart_tx(&UBX_CFG_MSGPVT[0], sizeof(UBX_CFG_MSGPVT));
	delay_ms(100);
	uart_tx(&UBX_CFG_RATE[0], sizeof(UBX_CFG_RATE));
	delay_ms(100);
	uart_tx(&UBX_CFG_CFG[0], sizeof(UBX_CFG_CFG));
}

void gps_parse(char c)
{
	switch(rx_cnt)
	{
	case 0:
		if(c == 0xB5)
		{
			m8n_rx_buf[rx_cnt] = c;
			rx_cnt++;
		}
		break;

	case 1:
		if(c == 0x62)
		{
			m8n_rx_buf[rx_cnt] = c;
			rx_cnt++;
		}
		else
		{
			rx_cnt = 0;
		}
		break;

	case 99 - 8:
		m8n_rx_buf[rx_cnt] = c;
		rx_cnt = 0;
		m8n_rx_cplt_flag = true;
		break;

	default:
		m8n_rx_buf[rx_cnt] = c;
		rx_cnt++;
		break;
	}
}

unsigned char M8N_UBX_CHKSUM_Check(unsigned char *data, unsigned char len)
{
	unsigned char CK_A = 0, CK_B = 0;
	for(int i = 2; i < len - 2; i++)
	{
		CK_A = CK_A + data[i];
		CK_B = CK_B + CK_A;
	}
	return ((CK_A == data[len - 2]) && (CK_B == data[len - 1]));
}

void M8N_UBX_NAV_PVT_Parsing(unsigned char *data, M8N_UBX_NAV_PVT *p)
{
	p->CLASS = data[2];
	p->ID = data[3];
	p->length = data[4] | data[5] << 8;

	p->iTOW = data[6] | data[7] << 8 | data[8] << 16 | data[9] << 24;
	p->year = data[10] | data[11] << 8;
	p->month = data[12];
	p->day = data[13];
	p->hour = data[14];
	p->min = data[15];
	p->sec = data[16];
	p->valid = data[17];
	p->tAcc = data[18] | data[19] << 8 | data[20] << 16 | data[21] << 24;
	p->nano = data[22] | data[23] << 8 | data[24] << 16 | data[25] << 24;
	p->fixType = data[26];
	p->flags = data[27];
	p->flags2 = data[28];
	p->numSV = data[29];
	p->lon = data[30] | data[31] << 8 | data[32] << 16 | data[33] << 24;
	p->lat = data[34] | data[35] << 8 | data[36] << 16 | data[37] << 24;
	p->height = data[38] | data[39] << 8 | data[40] << 16 | data[41] << 24;
	p->hMSL = data[42] | data[43] << 8 | data[44] << 16 | data[45] << 24;
	p->hAcc = data[46] | data[47] << 8 | data[48] << 16 | data[49] << 24;
	p->vAcc = data[50] | data[51] << 8 | data[52] << 16 | data[53] << 24;
	p->velN = data[54] | data[55] << 8 | data[56] << 16 | data[57] << 24;
	p->velE = data[58] | data[59] << 8 | data[60] << 16 | data[61] << 24;
	p->velD = data[62] | data[63] << 8 | data[64] << 16 | data[65] << 24;
	p->gSpeed = data[66] | data[67] << 8 | data[68] << 16 | data[69] << 24;
	p->headMot = data[70] | data[71] << 8 | data[72] << 16 | data[73] << 24;
	p->sAcc = data[74] | data[75] << 8 | data[76] << 16 | data[77] << 24;
	p->headAcc = data[78] | data[79] << 8 | data[80] << 16 | data[81] << 24;
	p->pDOP = data[82] | data[83] << 8;
	p->flags3 = data[84] | data[85] << 8;
	p->reserved1 = data[86] | data[87] << 8 | data[88] << 16 | data[89] << 24;
}

static void sync_od(void)
{
	OD_RAM.x6100_gps.year = pvt.year;
	OD_RAM.x6100_gps.month = pvt.month;
	OD_RAM.x6100_gps.day = pvt.day;
	OD_RAM.x6100_gps.hour = pvt.hour;
	OD_RAM.x6100_gps.min = pvt.min;
	OD_RAM.x6100_gps.sec = pvt.sec;
	OD_RAM.x6100_gps.nano = pvt.nano;
	OD_RAM.x6100_gps.iTOW = pvt.iTOW;
	OD_RAM.x6100_gps.tAcc = pvt.tAcc;
	OD_RAM.x6100_gps.lon = pvt.lon;
	OD_RAM.x6100_gps.lat = pvt.lat;
	OD_RAM.x6100_gps.height = pvt.height;
	OD_RAM.x6100_gps.hMSL = pvt.hMSL;
	OD_RAM.x6100_gps.hAcc = pvt.hAcc;
	OD_RAM.x6100_gps.vAcc = pvt.vAcc;
	OD_RAM.x6100_gps.sAcc = pvt.sAcc;
	OD_RAM.x6100_gps.headAcc = pvt.headAcc;
	OD_RAM.x6100_gps.numSV = pvt.numSV;
	OD_RAM.x6100_gps.headMot = pvt.headMot;
	OD_RAM.x6100_gps.velN = pvt.velN;
	OD_RAM.x6100_gps.velE = pvt.velE;
	OD_RAM.x6100_gps.velD = pvt.velD;
	OD_RAM.x6100_gps.gSpeed = pvt.gSpeed;
	OD_RAM.x6100_gps.pDOP = pvt.pDOP;
	gps_flags_t f;
	f.fix = pvt.fixType & 0x7;
	f.val_date = (pvt.valid & (1 << 0)) ? 1 : 0;
	f.val_time = (pvt.valid & (1 << 1)) ? 1 : 0;
	f.fully_resolved = (pvt.valid & (1 << 2)) ? 1 : 0;
	f.valid_mag = (pvt.valid & (1 << 3)) ? 1 : 0;
	f.gnss_fix_ok = (pvt.flags & (1 << 0)) ? 1 : 0;
	f.diff_soln = (pvt.flags & (1 << 1)) ? 1 : 0;
	f.head_veh_valid = (pvt.flags & (1 << 5)) ? 1 : 0;
	f.carr_soln = (pvt.flags & (1 << 7)) ? 1 : 0;
	f.confirmed_avai = (pvt.flags2 & (1 << 5)) ? 1 : 0;
	f.confirmed_date = (pvt.flags2 & (1 << 6)) ? 1 : 0;
	f.confirmed_time = (pvt.flags2 & (1 << 7)) ? 1 : 0;
	f.invalid_llh = (pvt.flags3 & (1 << 0)) ? 1 : 0;
	f.last_corr_age = (pvt.flags3 >> 1) & 0xF;
	OD_RAM.x6100_gps.flags = f.word;
}

void gps_poll(void)
{
	if(m8n_rx_cplt_flag == 1)
	{
		m8n_rx_cplt_flag = 0;

		if(M8N_UBX_CHKSUM_Check((uint8_t *)&m8n_rx_buf[0], 100 - 8) == 1)
		{
			M8N_UBX_NAV_PVT_Parsing((uint8_t *)&m8n_rx_buf[0], &pvt);

			sync_od();
		}
	}
}
