#include "adc.h"
#include "CANopen.h"
#include "OD.h"
#include "stm32f10x.h"

enum
{
	ADC_CH_VIN = 0,
	ADC_CH_SRV,
	ADC_CH_AUX,
	ADC_CH_I1,
	ADC_CH_I0,
	ADC_CH_AI0,
	ADC_CH_AI1,
	ADC_CH_AI2,
	ADC_CH_AI3,
	ADC_CH,
};

// adc_val_t adc_val = {0};

static volatile uint16_t adc_buf[ADC_CH];

void adc_init(void)
{
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	ADC_InitTypeDef adc_struct;
	adc_struct.ADC_Mode = ADC_Mode_Independent;
	adc_struct.ADC_DataAlign = ADC_DataAlign_Right;
	adc_struct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
	adc_struct.ADC_ScanConvMode = ENABLE;
	adc_struct.ADC_ContinuousConvMode = DISABLE;
	adc_struct.ADC_NbrOfChannel = ADC_CH;
	ADC_Init(ADC1, &adc_struct);

	ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_239Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_239Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 3, ADC_SampleTime_239Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 4, ADC_SampleTime_239Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 5, ADC_SampleTime_239Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_5, 6, ADC_SampleTime_239Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_6, 7, ADC_SampleTime_239Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_7, 8, ADC_SampleTime_239Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_8, 9, ADC_SampleTime_239Cycles5);

	DMA_DeInit(DMA1_Channel1);
	DMA_InitTypeDef dma_struct;
	dma_struct.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
	dma_struct.DMA_MemoryBaseAddr = (uint32_t)adc_buf;
	dma_struct.DMA_DIR = DMA_DIR_PeripheralSRC;
	dma_struct.DMA_BufferSize = ADC_CH;
	dma_struct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	dma_struct.DMA_MemoryInc = DMA_MemoryInc_Enable;
	dma_struct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	dma_struct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	dma_struct.DMA_Mode = DMA_Mode_Normal;
	dma_struct.DMA_Priority = DMA_Priority_Medium;
	dma_struct.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel1, &dma_struct);
	DMA_Cmd(DMA1_Channel1, ENABLE);

	ADC_DMACmd(ADC1, ENABLE);
	ADC_Cmd(ADC1, ENABLE);

	ADC_ResetCalibration(ADC1);
	while(ADC_GetResetCalibrationStatus(ADC1))
		;
	ADC_StartCalibration(ADC1);
	while(ADC_GetCalibrationStatus(ADC1))
		;

	adc_trig_conv();
}

void adc_trig_conv(void)
{
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

void adc_track(void)
{
	if(DMA1_Channel1->CNDTR == 0)
	{
		{
			// adc_val.vin = adc_buf[ADC_CH_VIN];
			// adc_val.srv_pos = adc_buf[ADC_CH_SRV];
			// adc_val.sns_mq2 = adc_buf[ADC_CH_AUX];
			// adc_val.sns_i1 = adc_buf[ADC_CH_I1];
			// adc_val.sns_i0 = adc_buf[ADC_CH_I0];
			// adc_val.sns_ai0 = adc_buf[ADC_CH_AI0];
			// adc_val.sns_ai1 = adc_buf[ADC_CH_AI1];
			// adc_val.sns_ai2 = adc_buf[ADC_CH_AI2];
			// adc_val.sns_ai3 = adc_buf[ADC_CH_AI3];

			OD_RAM.x6000_adc.ai0 = adc_buf[ADC_CH_AI0];
			OD_RAM.x6000_adc.ai1 = adc_buf[ADC_CH_AI1];
			OD_RAM.x6000_adc.ai2 = adc_buf[ADC_CH_AI2];
			OD_RAM.x6000_adc.ai3 = adc_buf[ADC_CH_AI3];
			OD_RAM.x6000_adc.aux = adc_buf[ADC_CH_AUX];
			OD_RAM.x6000_adc.srv = adc_buf[ADC_CH_SRV];
			OD_RAM.x6000_adc.vin = adc_buf[ADC_CH_VIN];
			OD_RAM.x6000_adc.i0 = adc_buf[ADC_CH_I0];
			OD_RAM.x6000_adc.i1 = adc_buf[ADC_CH_I1];
		}
		DMA_Cmd(DMA1_Channel1, DISABLE);
		DMA1_Channel1->CNDTR = ADC_CH;
		DMA1_Channel1->CMAR = (uint32_t)adc_buf;
		DMA_Cmd(DMA1_Channel1, ENABLE);
		adc_trig_conv();
	}
}