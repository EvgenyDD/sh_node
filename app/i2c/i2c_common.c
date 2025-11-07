#include "i2c_common.h"
#include "stm32f10x.h"
#include <stdbool.h>

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
	GPIOB->CRH &= (uint32_t)~(1 << 3); // PB8
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

///////////////////////////////////////////////////

#define I2C_7BIT_ADD_READ(__ADDRESS__) ((uint8_t)((__ADDRESS__) | I2C_OAR1_ADD0))
#define I2C_7BIT_ADD_WRITE(__ADDRESS__) ((uint8_t)((__ADDRESS__) & (uint8_t)(~I2C_OAR1_ADD0)))
#define I2C_7BIT_ADD_READ(__ADDRESS__) ((uint8_t)((__ADDRESS__) | I2C_OAR1_ADD0))
#define I2C_MEM_ADD_MSB(__ADDRESS__) ((uint8_t)((uint16_t)(((uint16_t)((__ADDRESS__) & (uint16_t)0xFF00)) >> 8)))
#define I2C_MEM_ADD_LSB(__ADDRESS__) ((uint8_t)((uint16_t)((__ADDRESS__) & (uint16_t)0x00FF)))

#define I2C_TIMEOUT_FLAG 35U /*!< Timeout 35 ms             */
#define __HAL_I2C_CLEAR_ADDRFLAG()    \
	do                                \
	{                                 \
		__IO uint32_t tmpreg = 0x00U; \
		tmpreg = I2C1->SR1;           \
		tmpreg = I2C1->SR2;           \
		(void)(tmpreg);               \
	} while(0)

#define I2C_FLAG_MASK 0x0000FFFFU

#define __HAL_I2C_GET_FLAG(__FLAG__) ((((uint8_t)((__FLAG__) >> 16U)) == 0x01U)                                                            \
										  ? ((((I2C1->SR1) & ((__FLAG__) & I2C_FLAG_MASK)) == ((__FLAG__) & I2C_FLAG_MASK)) ? SET : RESET) \
										  : ((((I2C1->SR2) & ((__FLAG__) & I2C_FLAG_MASK)) == ((__FLAG__) & I2C_FLAG_MASK)) ? SET : RESET))

#define __HAL_I2C_CLEAR_FLAG(__FLAG__) (I2C1->SR1 = ~((__FLAG__) & I2C_FLAG_MASK))

static __IO uint32_t PreviousState = 0;

typedef enum
{
	HAL_I2C_MODE_NONE = 0x00U,	 /*!< No I2C communication on going             */
	HAL_I2C_MODE_MASTER = 0x10U, /*!< I2C communication is in Master Mode       */
	HAL_I2C_MODE_SLAVE = 0x20U,	 /*!< I2C communication is in Slave Mode        */
	HAL_I2C_MODE_MEM = 0x40U	 /*!< I2C communication is in Memory Mode       */
} HAL_I2C_ModeTypeDef;

typedef enum
{
	HAL_I2C_STATE_RESET = 0x00U,		  /*!< Peripheral is not yet Initialized         */
	HAL_I2C_STATE_READY = 0x20U,		  /*!< Peripheral Initialized and ready for use  */
	HAL_I2C_STATE_BUSY = 0x24U,			  /*!< An internal process is ongoing            */
	HAL_I2C_STATE_BUSY_TX = 0x21U,		  /*!< Data Transmission process is ongoing      */
	HAL_I2C_STATE_BUSY_RX = 0x22U,		  /*!< Data Reception process is ongoing         */
	HAL_I2C_STATE_LISTEN = 0x28U,		  /*!< Address Listen Mode is ongoing            */
	HAL_I2C_STATE_BUSY_TX_LISTEN = 0x29U, /*!< Address Listen Mode and Data Transmission
											  process is ongoing                         */
	HAL_I2C_STATE_BUSY_RX_LISTEN = 0x2AU, /*!< Address Listen Mode and Data Reception
											  process is ongoing                         */
	HAL_I2C_STATE_ABORT = 0x60U,		  /*!< Abort user request ongoing                */
	HAL_I2C_STATE_TIMEOUT = 0xA0U,		  /*!< Timeout state                             */
	HAL_I2C_STATE_ERROR = 0xE0U			  /*!< Error                                     */
} HAL_I2C_StateTypeDef;

#define HAL_I2C_ERROR_NONE 0x00000000U		/*!< No error              */
#define HAL_I2C_ERROR_BERR 0x00000001U		/*!< BERR error            */
#define HAL_I2C_ERROR_ARLO 0x00000002U		/*!< ARLO error            */
#define HAL_I2C_ERROR_AF 0x00000004U		/*!< AF error              */
#define HAL_I2C_ERROR_OVR 0x00000008U		/*!< OVR error             */
#define HAL_I2C_ERROR_DMA 0x00000010U		/*!< DMA transfer error    */
#define HAL_I2C_ERROR_TIMEOUT 0x00000020U	/*!< Timeout Error         */
#define HAL_I2C_ERROR_SIZE 0x00000040U		/*!< Size Management error */
#define HAL_I2C_ERROR_DMA_PARAM 0x00000080U /*!< DMA Parameter Error   */
#define HAL_I2C_WRONG_START 0x00000200U		/*!< Wrong start Error     */

__IO HAL_I2C_StateTypeDef State = 0;
__IO HAL_I2C_ModeTypeDef Mode = 0;
__IO uint32_t ErrorCode = 0;

static int I2C_WaitOnRXNEFlagUntilTimeout(void)
{
	time = TIMEOUT;
	while(__HAL_I2C_GET_FLAG(I2C_FLAG_RXNE) == RESET)
	{
		if(__HAL_I2C_GET_FLAG(I2C_FLAG_STOPF) == SET)
		{
			__HAL_I2C_CLEAR_FLAG(I2C_FLAG_STOPF);
			PreviousState = HAL_I2C_MODE_NONE;
			State = HAL_I2C_STATE_READY;
			Mode = HAL_I2C_MODE_NONE;
			return -1;
		}

		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(I2C_FLAG_RXNE) == RESET))
			{
				PreviousState = HAL_I2C_MODE_NONE;
				State = HAL_I2C_STATE_READY;
				Mode = HAL_I2C_MODE_NONE;
				ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
				return -1;
			}
		}
	}
	return 0;
}

static int I2C_IsAcknowledgeFailed(void)
{
	if(__HAL_I2C_GET_FLAG(I2C_FLAG_AF) == SET)
	{
		__HAL_I2C_CLEAR_FLAG(I2C_FLAG_AF); /* Clear NACKF Flag */
		PreviousState = HAL_I2C_MODE_NONE;
		State = HAL_I2C_STATE_READY;
		Mode = HAL_I2C_MODE_NONE;
		ErrorCode |= HAL_I2C_ERROR_AF;
		return -1;
	}
	return 0;
}

static int I2C_WaitOnTXEFlagUntilTimeout(void)
{
	time = TIMEOUT;
	while(__HAL_I2C_GET_FLAG(I2C_FLAG_TXE) == RESET)
	{
		if(I2C_IsAcknowledgeFailed() != 0) return -1;
		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(I2C_FLAG_TXE) == RESET))
			{
				PreviousState = HAL_I2C_MODE_NONE;
				State = HAL_I2C_STATE_READY;
				Mode = HAL_I2C_MODE_NONE;
				ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
				return -1;
			}
		}
	}
	return 0;
}

static int I2C_WaitOnBTFFlagUntilTimeout(void)
{
	time = TIMEOUT;
	while(__HAL_I2C_GET_FLAG(I2C_FLAG_BTF) == RESET)
	{
		if(I2C_IsAcknowledgeFailed() != 0) return -1;
		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(I2C_FLAG_BTF) == RESET))
			{
				PreviousState = HAL_I2C_MODE_NONE;
				State = HAL_I2C_STATE_READY;
				Mode = HAL_I2C_MODE_NONE;
				ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
				return -1;
			}
		}
	}
	return 0;
}

static int I2C_WaitOnMasterAddressFlagUntilTimeout(uint32_t Flag)
{
	time = TIMEOUT;
	while(__HAL_I2C_GET_FLAG(Flag) == RESET)
	{
		if(__HAL_I2C_GET_FLAG(I2C_FLAG_AF) == SET)
		{
			SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			__HAL_I2C_CLEAR_FLAG(I2C_FLAG_AF);
			PreviousState = HAL_I2C_MODE_NONE;
			State = HAL_I2C_STATE_READY;
			Mode = HAL_I2C_MODE_NONE;
			ErrorCode |= HAL_I2C_ERROR_AF;
			return -1;
		}

		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(Flag) == RESET))
			{
				PreviousState = HAL_I2C_MODE_NONE;
				State = HAL_I2C_STATE_READY;
				Mode = HAL_I2C_MODE_NONE;
				ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
				return -1;
			}
		}
	}
	return 0;
}

static int I2C_WaitOnFlagUntilTimeout(uint32_t Flag, bool Status)
{
	time = TIMEOUT;
	while(__HAL_I2C_GET_FLAG(Flag) == Status)
	{
		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(Flag) == Status))
			{
				PreviousState = HAL_I2C_MODE_NONE;
				State = HAL_I2C_STATE_READY;
				Mode = HAL_I2C_MODE_NONE;
				ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
				return -1;
			}
		}
	}
	return 0;
}

static int I2C_RequestMemoryWrite(uint16_t addr, uint16_t MemAddress, bool is_mem_16_bit)
{
	SET_BIT(I2C1->CR1, I2C_CR1_START);
	if(I2C_WaitOnFlagUntilTimeout(I2C_FLAG_SB, RESET) != 0)
	{
		if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
		return -2;
	}

	I2C1->DR = I2C_7BIT_ADD_WRITE(addr);

	if(I2C_WaitOnMasterAddressFlagUntilTimeout(I2C_FLAG_ADDR) != 0) return -1;

	__HAL_I2C_CLEAR_ADDRFLAG();

	if(I2C_WaitOnTXEFlagUntilTimeout() != 0)
	{
		if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
		return -1;
	}

	if(is_mem_16_bit == false)
	{
		I2C1->DR = I2C_MEM_ADD_LSB(MemAddress);
	}
	else // 16bit
	{
		I2C1->DR = I2C_MEM_ADD_MSB(MemAddress);
		if(I2C_WaitOnTXEFlagUntilTimeout() != 0)
		{
			if(ErrorCode == HAL_I2C_ERROR_AF)
			{
				SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			}
			return -1;
		}
		I2C1->DR = I2C_MEM_ADD_LSB(MemAddress);
	}

	return 0;
}

static int I2C_RequestMemoryRead(uint16_t addr, uint16_t MemAddress, bool is_mem_16_bit)
{
	SET_BIT(I2C1->CR1, I2C_CR1_ACK);
	SET_BIT(I2C1->CR1, I2C_CR1_START);
	if(I2C_WaitOnFlagUntilTimeout(I2C_FLAG_SB, RESET) != 0)
	{
		if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
		return -2;
	}

	I2C1->DR = I2C_7BIT_ADD_WRITE(addr);

	if(I2C_WaitOnMasterAddressFlagUntilTimeout(I2C_FLAG_ADDR) != 0) return -1;

	__HAL_I2C_CLEAR_ADDRFLAG();

	if(I2C_WaitOnTXEFlagUntilTimeout() != 0)
	{
		if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
		return -1;
	}

	if(is_mem_16_bit == false)
	{
		I2C1->DR = I2C_MEM_ADD_LSB(MemAddress);
	}
	else // 16bit
	{
		I2C1->DR = I2C_MEM_ADD_MSB(MemAddress);
		if(I2C_WaitOnTXEFlagUntilTimeout() != 0)
		{
			if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			return -1;
		}
		I2C1->DR = I2C_MEM_ADD_LSB(MemAddress);
	}

	if(I2C_WaitOnTXEFlagUntilTimeout() != 0)
	{
		if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
		return -1;
	}

	SET_BIT(I2C1->CR1, I2C_CR1_START);

	if(I2C_WaitOnFlagUntilTimeout(I2C_FLAG_SB, RESET) != 0)
	{
		if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
		return -2;
	}

	I2C1->DR = I2C_7BIT_ADD_READ(addr);
	if(I2C_WaitOnMasterAddressFlagUntilTimeout(I2C_FLAG_ADDR) != 0) return -1;

	return 0;
}

///////////////////////////////////////////////////

int HAL_I2C_Mem_Write(uint16_t addr, uint16_t MemAddress, bool is_mem_16_bit, const uint8_t *pData, uint16_t Size)
{
	if(I2C_WaitOnFlagUntilTimeout(I2C_FLAG_BUSY, SET) != 0) return -2;

	CLEAR_BIT(I2C1->CR1, I2C_CR1_POS);

	State = HAL_I2C_STATE_BUSY_TX;
	Mode = HAL_I2C_MODE_MEM;
	ErrorCode = HAL_I2C_ERROR_NONE;

	const uint8_t *pBuffPtr = pData;
	uint8_t XferCount = Size;
	uint8_t XferSize = XferCount;

	if(I2C_RequestMemoryWrite(addr, MemAddress, is_mem_16_bit) != 0) return -1;

	while(XferSize > 0U)
	{
		if(I2C_WaitOnTXEFlagUntilTimeout() != 0)
		{
			if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			return -1;
		}

		I2C1->DR = *pBuffPtr;
		pBuffPtr++;
		XferSize--;
		XferCount--;

		if((__HAL_I2C_GET_FLAG(I2C_FLAG_BTF) == SET) && (XferSize != 0U))
		{
			I2C1->DR = *pBuffPtr;
			pBuffPtr++;
			XferSize--;
			XferCount--;
		}
	}

	if(I2C_WaitOnBTFFlagUntilTimeout() != 0)
	{
		if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
		return -1;
	}

	SET_BIT(I2C1->CR1, I2C_CR1_STOP);
	State = HAL_I2C_STATE_READY;
	Mode = HAL_I2C_MODE_NONE;
	return 0;
}

int HAL_I2C_Mem_Read(uint16_t addr, uint16_t MemAddress, bool is_mem_16_bit, uint8_t *pData, uint16_t Size)
{
	__IO uint32_t count = 0U;

	if(I2C_WaitOnFlagUntilTimeout(I2C_FLAG_BUSY, SET) != 0) return -2;

	CLEAR_BIT(I2C1->CR1, I2C_CR1_POS);

	State = HAL_I2C_STATE_BUSY_RX;
	Mode = HAL_I2C_MODE_MEM;
	ErrorCode = HAL_I2C_ERROR_NONE;

	uint8_t *pBuffPtr = pData;
	uint32_t XferCount = Size;
	uint32_t XferSize = XferCount;

	if(I2C_RequestMemoryRead(addr, MemAddress, is_mem_16_bit) != 0) return -1;

	if(XferSize == 0U)
	{
		__HAL_I2C_CLEAR_ADDRFLAG();
		SET_BIT(I2C1->CR1, I2C_CR1_STOP);
	}
	else if(XferSize == 1U)
	{
		CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK);
		__disable_irq();
		__HAL_I2C_CLEAR_ADDRFLAG();
		SET_BIT(I2C1->CR1, I2C_CR1_STOP);
		__enable_irq();
	}
	else if(XferSize == 2U)
	{
		SET_BIT(I2C1->CR1, I2C_CR1_POS);
		__disable_irq();
		__HAL_I2C_CLEAR_ADDRFLAG();
		CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK);
		__enable_irq();
	}
	else
	{
		SET_BIT(I2C1->CR1, I2C_CR1_ACK);
		__HAL_I2C_CLEAR_ADDRFLAG();
	}

	while(XferSize > 0U)
	{
		if(XferSize <= 3U)
		{
			if(XferSize == 1U)
			{
				if(I2C_WaitOnRXNEFlagUntilTimeout() != 0) return -1;

				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
			else if(XferSize == 2U)
			{
				if(I2C_WaitOnFlagUntilTimeout(I2C_FLAG_BTF, RESET) != 0) return -1;
				__disable_irq();
				SET_BIT(I2C1->CR1, I2C_CR1_STOP);
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
				__enable_irq();
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
			else // 3 Last bytes
			{
				if(I2C_WaitOnFlagUntilTimeout(I2C_FLAG_BTF, RESET) != 0) return -1;

				CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK);
				__disable_irq();
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
				count = I2C_TIMEOUT_FLAG * (SystemCoreClock / 25U / 1000U);
				do
				{
					count--;
					if(count == 0U)
					{
						PreviousState = HAL_I2C_MODE_NONE;
						State = HAL_I2C_STATE_READY;
						Mode = HAL_I2C_MODE_NONE;
						ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
						__enable_irq();

						return -1;
					}
				} while(__HAL_I2C_GET_FLAG(I2C_FLAG_BTF) == RESET);

				SET_BIT(I2C1->CR1, I2C_CR1_STOP);
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;

				XferSize--;
				XferCount--;

				__enable_irq();

				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
		}
		else
		{
			if(I2C_WaitOnRXNEFlagUntilTimeout() != 0) return -1;

			*pBuffPtr = (uint8_t)I2C1->DR;
			pBuffPtr++;
			XferSize--;
			XferCount--;

			if(__HAL_I2C_GET_FLAG(I2C_FLAG_BTF) == SET)
			{
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
		}
	}

	State = HAL_I2C_STATE_READY;
	Mode = HAL_I2C_MODE_NONE;

	return 0;
}

int HAL_I2C_IsDeviceReady(uint16_t addr, uint32_t trials)
{
	uint32_t try = 0U;
	bool tmp1;
	bool tmp2;

	if(I2C_WaitOnFlagUntilTimeout(I2C_FLAG_BUSY, SET) != 0) return -2;

	CLEAR_BIT(I2C1->CR1, I2C_CR1_POS);
	State = HAL_I2C_STATE_BUSY;
	ErrorCode = HAL_I2C_ERROR_NONE;

	do
	{
		time = TIMEOUT_SHORT;

		SET_BIT(I2C1->CR1, I2C_CR1_START);
		if(I2C_WaitOnFlagUntilTimeout(I2C_FLAG_SB, RESET) != 0)
		{
			if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
			return -2;
		}

		I2C1->DR = I2C_7BIT_ADD_WRITE(addr);

		tmp1 = __HAL_I2C_GET_FLAG(I2C_FLAG_ADDR);
		tmp2 = __HAL_I2C_GET_FLAG(I2C_FLAG_AF);
		while((State != HAL_I2C_STATE_TIMEOUT) && (tmp1 == RESET) && (tmp2 == RESET))
		{
			if(--time == 0)
			{
				State = HAL_I2C_STATE_TIMEOUT;
			}
			tmp1 = __HAL_I2C_GET_FLAG(I2C_FLAG_ADDR);
			tmp2 = __HAL_I2C_GET_FLAG(I2C_FLAG_AF);
		}

		State = HAL_I2C_STATE_READY;

		if(__HAL_I2C_GET_FLAG(I2C_FLAG_ADDR) == SET)
		{
			SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			__HAL_I2C_CLEAR_ADDRFLAG();
			if(I2C_WaitOnFlagUntilTimeout(I2C_FLAG_BUSY, SET) != 0) return -1;
			State = HAL_I2C_STATE_READY;
			return 0;
		}
		else
		{
			SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			__HAL_I2C_CLEAR_FLAG(I2C_FLAG_AF);
			if(I2C_WaitOnFlagUntilTimeout(I2C_FLAG_BUSY, SET) != 0) return -1;
		}
		try++;
	} while(try < trials);

	State = HAL_I2C_STATE_READY;

	return -1;
}