#ifndef ADC_H__
#define ADC_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	uint16_t vin, srv_pos, sns_mq2, sns_i[2], sns_ai[4];
	int16_t t_mcu; // 0.1C
} adc_val_t;

void adc_init(void);
void adc_trig_conv(void);
bool adc_track(void);

int32_t ntc10k_adc_to_degc(int32_t adc_norm);

extern adc_val_t adc_val;

#endif // ADC_H__