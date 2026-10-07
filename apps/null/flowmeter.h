#ifndef FLOWMETER_H__
#define FLOWMETER_H__

#include <stdint.h>

void flowmeter_init(void);
void flowmeter_poll(uint32_t diff_ms);

#endif // FLOWMETER_H__