#ifndef DSM501_H__
#define DSM501_H__

#include <stdint.h>

typedef struct
{
	uint32_t acc_data_ms;
	uint32_t a;
} dsm501_data_t;

void dsm501_init(void);
void dsm501_poll(uint32_t diff_ms);

extern dsm501_data_t dsm501_data;

#endif // DSM501_H__