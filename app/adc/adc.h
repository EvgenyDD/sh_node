#ifndef ADC_H__
#define ADC_H__

#include <stdint.h>

typedef struct
{
	uint16_t vin, srv_pos, sns_mq2, sns_i0, sns_i1, sns_ai0, sns_ai1, sns_ai2, sns_ai3;
} adc_val_t;

void adc_init(void);
void adc_trig_conv(void);
void adc_track(void);

extern adc_val_t adc_val;

#endif // ADC_H__