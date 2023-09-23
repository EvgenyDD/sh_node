#ifndef APPS_H__
#define APPS_H__

#include <stdint.h>

void meteo_init(void);
void meteo_poll(uint32_t diff_ms);

#endif // APPS_H__