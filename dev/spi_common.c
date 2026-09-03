#include "spi_common.h"
#include "stm32f10x.h"

void spi_common_init(void)
{
	// B3 clk B4 miso B5 mosi A15 cs baro

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

	AFIO->MAPR |= AFIO_MAPR_SPI1_REMAP;

	SPI_InitTypeDef SPI_InitStructure;
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
	SPI_InitStructure.SPI_CRCPolynomial = 7;
	SPI_Init(SPI1, &SPI_InitStructure);
	SPI_SSOutputCmd(SPI1, DISABLE);
	SPI_Cmd(SPI1, ENABLE);
}

void spi_trx(const uint8_t *data_tx, uint8_t *data_rx, uint32_t sz)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, DISABLE); // see errata
	for(uint32_t i = 0; i < sz; i++)
	{
		while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET)
			;
		SPI_I2S_SendData(SPI1, (uint16_t)data_tx[i]);
		while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET)
			;
		data_rx[i] = SPI_I2S_ReceiveData(SPI1);
	}
	while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY))
		;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE); // see errata
}