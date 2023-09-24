#ifndef APPS_H__
#define APPS_H__

#include <stdint.h>

enum{
	HEAD_E = 0,
	HEAD_NE,
	HEAD_N,
	HEAD_NW,
	HEAD_W,
	HEAD_SW,
	HEAD_S,
	HEAD_SE,
};

void meteo_init(void);
void meteo_poll(uint32_t diff_ms);

#endif // APPS_H__