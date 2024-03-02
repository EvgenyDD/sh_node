#ifndef GPS_H__
#define GPS_H__

#include <stdint.h>

typedef struct
{
	uint8_t CLASS;
	uint8_t ID;
	uint16_t length;

	uint32_t iTOW;		// ms, GPS time of week of the navigation epoch
	uint16_t year;		// year (UTC)
	uint8_t month;		// month, range 1..12 (UTC)
	uint8_t day;		// day of month, range 1..31 (UTC)
	uint8_t hour;		// hour of day, range 0..23 (UTC)
	uint8_t min;		// minute of hour, range 0..59 (UTC)
	uint8_t sec;		// seconds of minute, range 0..60 (UTC)
	uint8_t valid;		// 4 // validity flags, 0 - valid_date, 1 - valid_time, 2 - fully_resolved, 3 - valid_mag
	uint32_t tAcc;		// ns, time accuracy estimate (UTC)
	int32_t nano;		// ns, fraction of second, range -1e9 .. 1e9 (UTC)
	uint8_t fixType;	// 0 - no_fix, 1 - dead reckoning only, 2 - 2D, 3 - 3D, 4 - GNSS + dead reckoning combined, 5 - time only fix
	uint8_t flags;		//
	uint8_t flags2;		//
	uint8_t numSV;		// number of satellites used in Nav Solution
	int32_t lon;		// 1e-7 deg, longitude
	int32_t lat;		// 1e-7 deg, latitude
	int32_t height;		// mm, height above ellipsoid
	int32_t hMSL;		// mm, height above mean sea level
	uint32_t hAcc;		// mm, horizontal accuracy estimate
	uint32_t vAcc;		// mm, vertical accuracy estimate
	int32_t velN;		// mm/s, NED north velocity
	int32_t velE;		// mm/s, NED east velocity
	int32_t velD;		// mm/s, NED down velocity
	int32_t gSpeed;		// mm/s, ground Speed (2-D)
	int32_t headMot;	// 1e-5 deg, heading of motion (2-D)
	uint32_t sAcc;		// mm/s, speed accuracy estimate
	uint32_t headAcc;	// 1e-5 deg, heading accuracy estimate (both motion & vehicle)
	uint16_t pDOP;		// 0.01, position DOP
	uint16_t flags3;	//
	uint32_t reserved1; //
} M8N_UBX_NAV_PVT;

typedef union
{
	struct
	{
		uint32_t fix : 3;
		uint32_t val_date : 1;
		uint32_t val_time : 1;
		uint32_t fully_resolved : 1;
		uint32_t valid_mag : 1;
		uint32_t invalid_llh : 1;
		uint32_t last_corr_age : 4;
		uint32_t gnss_fix_ok : 1;
		uint32_t diff_soln : 1;
		uint32_t head_veh_valid : 1;
		uint32_t carr_soln : 1;
		uint32_t confirmed_avai : 1;
		uint32_t confirmed_date : 1;
		uint32_t confirmed_time : 1;
	} s;
	uint32_t word;
} gps_flags_t;

void gps_init(void);
void gps_poll(void);
void gps_parse(char c);

#endif // GPS_H__