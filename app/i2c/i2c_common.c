#include "i2c_common.h"
#include "stm32f10x.h"

// https://github.com/meng4036/stm32_i2c/tree/ad7ab7ccd97991d0bd0be7dc739f7a015afeb25b

#define TIMEOUT 0xFFF
#define TIMEOUT_SHORT 100

/* --EV7_2 */
#define I2C_EVENT_MASTER_BYTE_BTF ((uint32_t)0x00030004) /* BUSY, MSL and BTF flags */

static uint32_t time = 0;

void i2c_common_init(void)
{
	GPIO_InitTypeDef GPIO_InitTypeStruct;
	I2C_InitTypeDef I2C_InitTypeStruct;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	DBGMCU->CR |= DBGMCU_CR_DBG_I2C1_SMBUS_TIMEOUT;

	GPIO_InitTypeStruct.GPIO_Mode = GPIO_Mode_AF_OD;
	GPIO_InitTypeStruct.GPIO_Pin = GPIO_Pin_8 /* SCL */ | GPIO_Pin_9 /* SDA */;
	GPIO_InitTypeStruct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &GPIO_InitTypeStruct);

	AFIO->MAPR |= AFIO_MAPR_I2C1_REMAP;

	RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, ENABLE);
	RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, DISABLE);

	I2C_DeInit(I2C1);

	I2C_InitTypeStruct.I2C_Ack = I2C_Ack_Enable;
	I2C_InitTypeStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitTypeStruct.I2C_ClockSpeed = 400000;
	I2C_InitTypeStruct.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitTypeStruct.I2C_Mode = I2C_Mode_I2C;
	I2C_InitTypeStruct.I2C_OwnAddress1 = 0;
	I2C_Init(I2C1, &I2C_InitTypeStruct);

	I2C_Cmd(I2C1, ENABLE);
}

static __inline void I2C_PullDownSCL(void)
{
	GPIOB->BRR = GPIO_Pin_8;
	GPIOB->CRH &= (uint32_t) ~(1 << 3); // PB8
}

static __inline void I2C_ReleaseSCL(void)
{
	GPIOB->CRH |= (1 << 3); // PB8
}

#define CHK_EVT_RET(x, c)                      \
	for(time = TIMEOUT; time > 0 && x; time--) \
		;                                      \
	if(time == 0) return (c)

#define CHK_EVT_RET_SHORT(x, c)                      \
	for(time = TIMEOUT_SHORT; time > 0 && x; time--) \
		;                                            \
	if(time == 0) return (c)

// int eeprom_write_byte(uint8_t addr, uint16_t reg, uint8_t data)
// {
// 	I2C_GenerateSTART(I2C1, ENABLE);											  // start
// 	CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT) == ERROR, -5); // EVENT 5

// 	I2C_Send7bitAddress(I2C1, addr << 1, I2C_Direction_Transmitter);							// send address
// 	CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == ERROR, -6); // EVENT 6

// 	I2C_SendData(I2C1, reg);														   // send eeprom reg
// 	CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED) == ERROR, -8); // EVENT 8

// 	I2C_SendData(I2C1, data);														   // send data
// 	CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED) == ERROR, -8); // EVENT 8

// 	I2C_GenerateSTOP(I2C1, ENABLE); // stop
// 	return 0;
// }

int i2c_common_write_to_read(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t size)
{
	CHK_EVT_RET(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY), -1);

	I2C_GenerateSTART(I2C1, ENABLE);											  // start
	CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT) == ERROR, -5); // EVENT 5

	I2C_Send7bitAddress(I2C1, addr << 1, I2C_Direction_Transmitter);							// send iic reg
	CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == ERROR, -6); // EVENT 6
	I2C_Cmd(I2C1, ENABLE);																		// clear EV6 by setting again the PE bit

	I2C_SendData(I2C1, reg);														   // send eeprom reg
	CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED) == ERROR, -8); // EVENT 8_2
	I2C_Cmd(I2C1, ENABLE);

	I2C_GenerateSTART(I2C1, ENABLE);											   // re-start
	CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT) == ERROR, -50); // EVENT 5

	I2C_Send7bitAddress(I2C1, addr << 1, I2C_Direction_Receiver); // send iic reg

	uint8_t i = 0;
	volatile uint8_t a;

	if(size == 1)
	{
#if 0
		while(!I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR)) // EVENT 6
			;

		I2C_PullDownSCL();
		
		I2C_AcknowledgeConfig(I2C1, DISABLE);

		__DMB();
		a = I2C1->SR1;
		a = I2C1->SR2;
		__DMB();

		I2C_GenerateSTOP(I2C1, ENABLE);
		I2C_ReleaseSCL();

		while(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED))
			;
		a = I2C_ReceiveData(I2C1);
		while(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED))
			;
		data[i++] = I2C_ReceiveData(I2C1);
#else
		while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
			;
		I2C_AcknowledgeConfig(I2C1, DISABLE);
		I2C_GenerateSTOP(I2C1, ENABLE);
		while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED))
			;
		data[i++] = I2C_ReceiveData(I2C1);
#endif
		return 0;
	}
	else if(size == 2)
	{
		while(!I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR)) // EVENT 6
			;

		I2C_AcknowledgeConfig(I2C1, ENABLE); // set POS and ACK
		I2C1->CR1 |= 0x0800;

		I2C_PullDownSCL();

		__DMB();
		a = I2C1->SR1;
		a = I2C1->SR2;
		__DMB();
		I2C_AcknowledgeConfig(I2C1, DISABLE);

		I2C_ReleaseSCL();

		while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_BTF))
			;

		I2C_PullDownSCL();

		I2C1->CR1 &= (uint32_t)~0x0800;
		I2C_AcknowledgeConfig(I2C1, ENABLE);
		I2C_GenerateSTOP(I2C1, ENABLE);
		data[i++] = I2C_ReceiveData(I2C1);

		I2C_ReleaseSCL();

		data[i++] = I2C_ReceiveData(I2C1);
		size -= 2;

		return 0;
	}
	else if(size > 2)
	{
		CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) == ERROR, -60); // EVENT 6

		while(size > 3)
		{
			while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_BTF))
				;
			data[i++] = I2C_ReceiveData(I2C1);
			size--;
		}

		while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_BTF))
			;
		I2C_PullDownSCL();
		I2C_AcknowledgeConfig(I2C1, DISABLE);
		I2C_ReleaseSCL();

		data[i++] = I2C_ReceiveData(I2C1);
		size--;

		while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_BTF))
			;

		I2C_PullDownSCL();

		I2C_AcknowledgeConfig(I2C1, ENABLE);
		I2C_GenerateSTOP(I2C1, ENABLE);
		data[i++] = I2C_ReceiveData(I2C1);

		I2C_ReleaseSCL();

		data[i++] = I2C_ReceiveData(I2C1);
		size -= 2;
	}

	return 0;
}

int i2c_common_read(uint8_t addr, uint8_t *data, uint16_t size)
{
	CHK_EVT_RET(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY), -1);

	I2C_GenerateSTART(I2C1, ENABLE);											   // start
	CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT) == ERROR, -50); // EVENT 5

	I2C_Send7bitAddress(I2C1, addr << 1, I2C_Direction_Receiver); // send iic reg

	uint8_t i = 0;
	volatile uint8_t a;

	if(size == 1)
	{
		CHK_EVT_RET(!I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR), -60); // EVENT 6

		I2C_AcknowledgeConfig(I2C1, DISABLE);

		__disable_irq();
		(void)I2C1->SR2;
		I2C_GenerateSTOP(I2C1, ENABLE);
		__enable_irq();

		CHK_EVT_RET(!I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE), -70);
		data[i++] = I2C_ReceiveData(I2C1);

		return 0;
	}
	else if(size == 2)
	{
		CHK_EVT_RET(!I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR), -60); // EVENT 6

		I2C_AcknowledgeConfig(I2C1, ENABLE); // set POS and ACK
		I2C1->CR1 |= 0x0800;

		I2C_PullDownSCL();

		__DMB();
		a = I2C1->SR1;
		a = I2C1->SR2;
		__DMB();
		I2C_AcknowledgeConfig(I2C1, DISABLE);

		I2C_ReleaseSCL();

		while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_BTF))
			;

		I2C_PullDownSCL();

		I2C1->CR1 &= (uint32_t)~0x0800;
		I2C_AcknowledgeConfig(I2C1, ENABLE);
		I2C_GenerateSTOP(I2C1, ENABLE);
		data[i++] = I2C_ReceiveData(I2C1);

		I2C_ReleaseSCL();

		data[i++] = I2C_ReceiveData(I2C1);
		size -= 2;

		return 0;
	}
	else if(size > 2)
	{
		CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) == ERROR, -60); // EVENT 6

		while(size > 3)
		{
			while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_BTF))
				;
			data[i++] = I2C_ReceiveData(I2C1);
			size--;
		}

		while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_BTF))
			;
		I2C_PullDownSCL();
		I2C_AcknowledgeConfig(I2C1, DISABLE);
		I2C_ReleaseSCL();

		data[i++] = I2C_ReceiveData(I2C1);
		size--;

		while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_BTF))
			;

		I2C_PullDownSCL();

		I2C_AcknowledgeConfig(I2C1, ENABLE);
		I2C_GenerateSTOP(I2C1, ENABLE);
		data[i++] = I2C_ReceiveData(I2C1);

		I2C_ReleaseSCL();

		data[i++] = I2C_ReceiveData(I2C1);
		size -= 2;
	}

	return 0;
}

int i2c_common_write(uint8_t addr, uint8_t reg, const uint8_t *data, uint16_t size)
{
	I2C_GenerateSTART(I2C1, ENABLE);											  // start
	CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT) == ERROR, -5); // EVENT 5

	I2C_Send7bitAddress(I2C1, addr << 1, I2C_Direction_Transmitter);							// send iic reg
	CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == ERROR, -6); // EVENT 6

	I2C_SendData(I2C1, reg);														   // send reg
	CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED) == ERROR, -8); // EVENT 8

	if(size)
	{
		while(size--)
		{
			I2C_SendData(I2C1, *data);
			data++;
			CHK_EVT_RET(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED) == ERROR, -80); // EVENT 8
		}
	}

	I2C_GenerateSTOP(I2C1, ENABLE);
	return 0;
}

int i2c_common_check_addr(uint8_t addr)
{
	I2C_GenerateSTART(I2C1, ENABLE);													// start
	CHK_EVT_RET_SHORT(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT) == ERROR, -5); // EVENT 5

	I2C_Send7bitAddress(I2C1, addr << 1, I2C_Direction_Transmitter);								  // send iic reg
	CHK_EVT_RET_SHORT(I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == ERROR, -6); // EVENT 6

	I2C_GenerateSTOP(I2C1, ENABLE);
	return 0;
}