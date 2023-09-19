#ifndef GPS_H__
#define GPS_H__

typedef struct
{
	unsigned char CLASS;
	unsigned char ID;
	unsigned short length;

	unsigned int iTOW;
	signed int lon;
	signed int lon_prev;
	signed int lat;
	signed int lat_prev;
	signed int height;
	signed int hMSL;
	unsigned int hAcc;
	unsigned int vAcc;

	double lon_f64;
	double lat_f64;
} M8N_UBX_NAV_POSLLH;

typedef struct
{
	unsigned char CLASS;
	unsigned char ID;
	unsigned short length;

	unsigned int iTOW;
	unsigned short year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char min;
	unsigned char sec;
	unsigned char valid; // X1 date type
	unsigned int tAcc;
	signed int nano;
	unsigned char fixType;
	unsigned char flags;  // X1 date type
	unsigned char flags2; // X1 date type
	unsigned char numSV;
	signed int lon;
	signed int lon_prev;
	signed int lat;
	signed int lat_prev;
	signed int height;
	signed int hMSL;
	unsigned int hAcc;
	unsigned int vAcc;
	signed int velN;
	signed int velE;
	signed int velD;
	signed int gSpeed;
	signed int headMot;
	unsigned int sAcc;
	unsigned int headAcc;
	unsigned short pDOP;
	unsigned short flags3; // X2 date type
	unsigned int reserved1;
	signed int headVeh;
	signed short magDec;
	unsigned short magAcc;
} M8N_UBX_NAV_PVT;

void gps_init(void);
void gps_poll(void);

#endif // GPS_H__