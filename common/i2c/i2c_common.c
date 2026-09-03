#include "i2c_common.h"
#include "stm32f10x.h"

#define TIMEOUT 0xFFF
#define TIMEOUT_SHORT 100

static volatile uint32_t time = 0;

void i2c_init(void)
{
	// GPIO_InitTypeDef GPIO_InitTypeStruct;
	// I2C_InitTypeDef I2C_InitTypeStruct;

	// RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
	// RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	// DBGMCU->CR |= DBGMCU_CR_DBG_I2C2_SMBUS_TIMEOUT;

	// GPIO_InitTypeStruct.GPIO_Mode = GPIO_Mode_AF_OD;
	// GPIO_InitTypeStruct.GPIO_Pin = GPIO_Pin_10 /* SCL */ | GPIO_Pin_11 /* SDA */;
	// GPIO_InitTypeStruct.GPIO_Speed = GPIO_Speed_2MHz;
	// GPIO_Init(GPIOB, &GPIO_InitTypeStruct);

	// RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C2, ENABLE);
	// RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C2, DISABLE);

	// I2C_DeInit(I2C1);

	// I2C_InitTypeStruct.I2C_Ack = I2C_Ack_Enable;
	// I2C_InitTypeStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	// I2C_InitTypeStruct.I2C_ClockSpeed = 400000;
	// I2C_InitTypeStruct.I2C_DutyCycle = I2C_DutyCycle_2;
	// I2C_InitTypeStruct.I2C_Mode = I2C_Mode_I2C;
	// I2C_InitTypeStruct.I2C_OwnAddress1 = 0;
	// I2C_Init(I2C1, &I2C_InitTypeStruct);

	// I2C_Cmd(I2C1, ENABLE);
	RCC->APB1ENR |= RCC_APB1Periph_I2C1;
	RCC->APB2ENR |= RCC_APB2Periph_GPIOB;

	DBGMCU->CR |= DBGMCU_CR_DBG_I2C1_SMBUS_TIMEOUT;

	GPIOB->CRH &= ~(GPIO_CRH_MODE8 | GPIO_CRH_CNF8 | GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
	GPIOB->CRH |= (GPIO_CRH_MODE8_1 | GPIO_CRH_CNF8_0 | GPIO_CRH_CNF8_1);
	GPIOB->CRH |= (GPIO_CRH_MODE9_1 | GPIO_CRH_CNF9_0 | GPIO_CRH_CNF9_1);

	AFIO->MAPR |= AFIO_MAPR_I2C1_REMAP;

	RCC->APB1RSTR |= RCC_APB1Periph_I2C1;
	RCC->APB1RSTR &= ~RCC_APB1Periph_I2C1;

	I2C1->CR1 &= ~I2C_CR1_PE;
	I2C1->CR2 = (I2C1->CR2 & ~I2C_CR2_FREQ) | 32;
	I2C1->CCR = I2C_CCR_FS | 27;
	I2C1->TRISE = 11;
	I2C1->OAR1 = (1 << 14) | 0x00;
	I2C1->CR1 |= I2C_CR1_ACK;
	I2C1->CR1 |= I2C_CR1_PE;
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

#define HAL_I2C_ERROR_NONE 0x00000000U	  /*!< No error              */
#define HAL_I2C_ERROR_AF 0x00000004U	  /*!< AF error              */
#define HAL_I2C_ERROR_TIMEOUT 0x00000020U /*!< Timeout Error         */
#define HAL_I2C_WRONG_START 0x00000200U	  /*!< Wrong start Error     */

#define HAL_I2C_FLAG_AF 0x00010400U
#define HAL_I2C_FLAG_TXE 0x00010080U
#define HAL_I2C_FLAG_RXNE 0x00010040U
#define HAL_I2C_FLAG_STOPF 0x00010010U
#define HAL_I2C_FLAG_BTF 0x00010004U
#define HAL_I2C_FLAG_ADDR 0x00010002U
#define HAL_I2C_FLAG_SB 0x00010001U
#define HAL_I2C_FLAG_BUSY 0x00100002U

__IO uint32_t ErrorCode = 0;

static int I2C_WaitOnRXNEFlagUntilTimeout(void)
{
	time = TIMEOUT;
	while(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_RXNE) == RESET)
	{
		if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_STOPF) == SET)
		{
			__HAL_I2C_CLEAR_FLAG(HAL_I2C_FLAG_STOPF);
			return -1;
		}

		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_RXNE) == RESET))
			{
				ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
				return -1;
			}
		}
	}
	return 0;
}

static int I2C_IsAcknowledgeFailed(void)
{
	if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_AF) == SET)
	{
		__HAL_I2C_CLEAR_FLAG(HAL_I2C_FLAG_AF); /* Clear NACKF Flag */
		ErrorCode |= HAL_I2C_ERROR_AF;
		return -1;
	}
	return 0;
}

static int I2C_WaitOnTXEFlagUntilTimeout(void)
{
	time = TIMEOUT;
	while(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_TXE) == RESET)
	{
		if(I2C_IsAcknowledgeFailed()) return -1;
		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_TXE) == RESET))
			{
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
		if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_AF) == SET)
		{
			SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			__HAL_I2C_CLEAR_FLAG(HAL_I2C_FLAG_AF);
			ErrorCode |= HAL_I2C_ERROR_AF;
			return -1;
		}

		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(Flag) == RESET))
			{
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
	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_SB, RESET))
	{
		if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
		return -2;
	}

	I2C1->DR = I2C_7BIT_ADD_WRITE(addr);

	if(I2C_WaitOnMasterAddressFlagUntilTimeout(HAL_I2C_FLAG_ADDR)) return -1;

	__HAL_I2C_CLEAR_ADDRFLAG();

	if(I2C_WaitOnTXEFlagUntilTimeout())
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
		if(I2C_WaitOnTXEFlagUntilTimeout())
		{
			if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
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
	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_SB, RESET))
	{
		if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
		return -2;
	}

	I2C1->DR = I2C_7BIT_ADD_WRITE(addr);

	if(I2C_WaitOnMasterAddressFlagUntilTimeout(HAL_I2C_FLAG_ADDR)) return -1;

	__HAL_I2C_CLEAR_ADDRFLAG();

	if(I2C_WaitOnTXEFlagUntilTimeout())
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
		if(I2C_WaitOnTXEFlagUntilTimeout())
		{
			if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			return -1;
		}
		I2C1->DR = I2C_MEM_ADD_LSB(MemAddress);
	}

	if(I2C_WaitOnTXEFlagUntilTimeout())
	{
		if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
		return -1;
	}

	SET_BIT(I2C1->CR1, I2C_CR1_START); // Generate Restart

	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_SB, RESET))
	{
		if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
		return -2;
	}

	I2C1->DR = I2C_7BIT_ADD_READ(addr);
	if(I2C_WaitOnMasterAddressFlagUntilTimeout(HAL_I2C_FLAG_ADDR)) return -1;

	return 0;
}

static int I2C_MasterRequestWrite(uint16_t DevAddress)
{
	SET_BIT(I2C1->CR1, I2C_CR1_START); /* Generate Start */

	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_SB, RESET)) /* Wait until SB flag is set */
	{
		if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
		return -2;
	}

	I2C1->DR = I2C_7BIT_ADD_WRITE(DevAddress);
	if(I2C_WaitOnMasterAddressFlagUntilTimeout(HAL_I2C_FLAG_ADDR)) return -1; /* Wait until ADDR flag is set */
	return 0;
}

static int I2C_MasterRequestRead(uint16_t DevAddress)
{
	SET_BIT(I2C1->CR1, I2C_CR1_ACK);   /* Enable Acknowledge */
	SET_BIT(I2C1->CR1, I2C_CR1_START); /* Generate Start */

	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_SB, RESET)) /* Wait until SB flag is set */
	{
		if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
		return -2;
	}

	I2C1->DR = I2C_7BIT_ADD_READ(DevAddress);
	if(I2C_WaitOnMasterAddressFlagUntilTimeout(HAL_I2C_FLAG_ADDR)) return -1; /* Wait until ADDR flag is set */
	return 0;
}

static int I2C_WaitOnBTFFlagUntilTimeout(void)
{
	time = TIMEOUT;
	while(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == RESET)
	{
		if(I2C_IsAcknowledgeFailed()) return -1;
		if(--time == 0)
		{
			if((__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == RESET))
			{
				ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
				return -1;
			}
		}
	}
	return 0;
}

///////////////////////////////////////////////////

int i2c_mem_write(uint16_t addr, uint16_t MemAddress, bool is_mem_16_bit, const uint8_t *pData, uint16_t Size)
{
	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BUSY, SET)) return I2C_ERR_BUSY;

	CLEAR_BIT(I2C1->CR1, I2C_CR1_POS);
	ErrorCode = HAL_I2C_ERROR_NONE;
	const uint8_t *pBuffPtr = pData;
	uint32_t XferCount = Size;
	uint32_t XferSize = XferCount;

	if(I2C_RequestMemoryWrite(addr, MemAddress, is_mem_16_bit)) return I2C_ERR_GENERAL;

	while(XferSize > 0U)
	{
		if(I2C_WaitOnTXEFlagUntilTimeout())
		{
			if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			return I2C_ERR_GENERAL;
		}

		I2C1->DR = *pBuffPtr;
		pBuffPtr++;
		XferSize--;
		XferCount--;

		if((__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == SET) && (XferSize != 0U))
		{
			I2C1->DR = *pBuffPtr;
			pBuffPtr++;
			XferSize--;
			XferCount--;
		}
	}

	if(I2C_WaitOnBTFFlagUntilTimeout())
	{
		if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP);
		return I2C_ERR_GENERAL;
	}

	SET_BIT(I2C1->CR1, I2C_CR1_STOP);
	return 0;
}

int i2c_mem_read(uint16_t addr, uint16_t MemAddress, bool is_mem_16_bit, uint8_t *pData, uint16_t Size)
{
	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BUSY, SET)) return I2C_ERR_BUSY;

	CLEAR_BIT(I2C1->CR1, I2C_CR1_POS);
	ErrorCode = HAL_I2C_ERROR_NONE;
	uint8_t *pBuffPtr = pData;
	uint32_t XferCount = Size;
	uint32_t XferSize = XferCount;

	if(I2C_RequestMemoryRead(addr, MemAddress, is_mem_16_bit)) return I2C_ERR_GENERAL;

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
				if(I2C_WaitOnRXNEFlagUntilTimeout()) return I2C_ERR_GENERAL;
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
			else if(XferSize == 2U)
			{
				if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BTF, RESET)) return I2C_ERR_GENERAL;
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
				if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BTF, RESET)) return I2C_ERR_GENERAL;

				CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK);
				__disable_irq();
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
				__IO uint32_t count = I2C_TIMEOUT_FLAG * (SystemCoreClock / 25U / 1000U);
				do
				{
					count--;
					if(count == 0U)
					{
						ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
						__enable_irq();
						return I2C_ERR_GENERAL;
					}
				} while(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == RESET);

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
			if(I2C_WaitOnRXNEFlagUntilTimeout()) return I2C_ERR_GENERAL;

			*pBuffPtr = (uint8_t)I2C1->DR;
			pBuffPtr++;
			XferSize--;
			XferCount--;

			if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == SET)
			{
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
		}
	}

	return 0;
}

int i2c_is_device_ready(uint16_t addr, uint32_t trials)
{
	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BUSY, SET)) return I2C_ERR_BUSY;

	CLEAR_BIT(I2C1->CR1, I2C_CR1_POS);
	ErrorCode = HAL_I2C_ERROR_NONE;

	uint32_t try = 0U;
	do
	{
		time = TIMEOUT_SHORT;

		SET_BIT(I2C1->CR1, I2C_CR1_START);
		if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_SB, RESET))
		{
			if(READ_BIT(I2C1->CR1, I2C_CR1_START) == I2C_CR1_START) ErrorCode = HAL_I2C_WRONG_START;
			return -2;
		}

		I2C1->DR = I2C_7BIT_ADD_WRITE(addr);

		bool tmp1 = __HAL_I2C_GET_FLAG(HAL_I2C_FLAG_ADDR);
		bool tmp2 = __HAL_I2C_GET_FLAG(HAL_I2C_FLAG_AF);
		while(time && (tmp1 == RESET) && (tmp2 == RESET))
		{
			--time;
			tmp1 = __HAL_I2C_GET_FLAG(HAL_I2C_FLAG_ADDR);
			tmp2 = __HAL_I2C_GET_FLAG(HAL_I2C_FLAG_AF);
		}

		if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_ADDR) == SET)
		{
			SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			__HAL_I2C_CLEAR_ADDRFLAG();
			if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BUSY, SET)) return I2C_ERR_GENERAL;
			return 0;
		}
		else
		{
			SET_BIT(I2C1->CR1, I2C_CR1_STOP);
			__HAL_I2C_CLEAR_FLAG(HAL_I2C_FLAG_AF);
			if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BUSY, SET)) return I2C_ERR_GENERAL;
		}
		try++;
	} while(try < trials);

	return I2C_ERR_TIMEOUT;
}

int i2c_tx(uint16_t DevAddress, uint8_t *pData, uint16_t Size)
{
	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BUSY, SET)) return I2C_ERR_BUSY; /* Wait until BUSY flag is reset */

	CLEAR_BIT(I2C1->CR1, I2C_CR1_POS); /* Disable Pos */
	ErrorCode = HAL_I2C_ERROR_NONE;
	uint8_t *pBuffPtr = pData;
	uint32_t XferCount = Size;
	uint32_t XferSize = XferCount;

	if(I2C_MasterRequestWrite(DevAddress)) return I2C_ERR_GENERAL; /* Send Slave Address */
	__HAL_I2C_CLEAR_ADDRFLAG();

	while(XferSize > 0U)
	{
		if(I2C_WaitOnTXEFlagUntilTimeout()) /* Wait until TXE flag is set */
		{
			if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */
			return I2C_ERR_GENERAL;
		}

		I2C1->DR = *pBuffPtr;
		pBuffPtr++;
		XferCount--;
		XferSize--;

		if((__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == SET) && (XferSize != 0U))
		{
			I2C1->DR = *pBuffPtr;
			pBuffPtr++;
			XferCount--;
			XferSize--;
		}

		if(I2C_WaitOnBTFFlagUntilTimeout()) /* Wait until BTF flag is set */
		{
			if(ErrorCode == HAL_I2C_ERROR_AF) SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */
			return I2C_ERR_GENERAL;
		}
	}

	SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */
	return 0;
}

int i2c_rx(uint16_t DevAddress, uint8_t *pData, uint16_t Size)
{
	if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BUSY, SET)) return I2C_ERR_BUSY; /* Wait until BUSY flag is reset */

	CLEAR_BIT(I2C1->CR1, I2C_CR1_POS); /* Disable Pos */
	ErrorCode = HAL_I2C_ERROR_NONE;
	uint8_t *pBuffPtr = pData;
	uint32_t XferCount = Size;
	uint32_t XferSize = XferCount;

	if(I2C_MasterRequestRead(DevAddress)) return I2C_ERR_GENERAL; /* Send Slave Address */

	if(XferSize == 0U)
	{
		__HAL_I2C_CLEAR_ADDRFLAG();
		SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */
	}
	else if(XferSize == 1U)
	{
		CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK); /* Disable Acknowledge */
		__disable_irq();				   // Disable all active IRQs around ADDR clearing and STOP programming because the EV6_3 software sequence must complete before the current byte end of transfer
		__HAL_I2C_CLEAR_ADDRFLAG();
		SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */
		__enable_irq();
	}
	else if(XferSize == 2U)
	{
		SET_BIT(I2C1->CR1, I2C_CR1_POS); /* Enable Pos */
		__disable_irq();				 // Disable all active IRQs around ADDR clearing and STOP programming because the EV6_3 software sequence must complete before the current byte end of transfer
		__HAL_I2C_CLEAR_ADDRFLAG();
		CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK); /* Disable Acknowledge */
		__enable_irq();
	}
	else
	{
		SET_BIT(I2C1->CR1, I2C_CR1_ACK); /* Enable Acknowledge */
		__HAL_I2C_CLEAR_ADDRFLAG();
	}

	while(XferSize > 0U)
	{
		if(XferSize <= 3U)
		{
			if(XferSize == 1U)
			{
				if(I2C_WaitOnRXNEFlagUntilTimeout()) return I2C_ERR_GENERAL; /* Wait until RXNE flag is set */

				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
			else if(XferSize == 2U)
			{
				if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BTF, RESET)) return I2C_ERR_GENERAL; /* Wait until BTF flag is set */

				__disable_irq(); // Disable all active IRQs around ADDR clearing and STOP programming because the EV6_3 software sequence must complete before the current byte end of transfer

				SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */

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
			else /* 3 Last bytes */
			{
				if(I2C_WaitOnFlagUntilTimeout(HAL_I2C_FLAG_BTF, RESET)) return I2C_ERR_GENERAL; /* Wait until BTF flag is set */
				CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK);												/* Disable Acknowledge */

				__disable_irq(); // Disable all active IRQs around ADDR clearing and STOP programming because the EV6_3 software sequence must complete before the current byte end of transfer

				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;

				__IO uint32_t count = I2C_TIMEOUT_FLAG * (SystemCoreClock / 25U / 1000U); /* Wait until BTF flag is set */
				do
				{
					count--;
					if(count == 0U)
					{
						ErrorCode |= HAL_I2C_ERROR_TIMEOUT;
						__enable_irq();
						return I2C_ERR_GENERAL;
					}
				} while(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == RESET);

				SET_BIT(I2C1->CR1, I2C_CR1_STOP); /* Generate Stop */

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
			if(I2C_WaitOnRXNEFlagUntilTimeout()) return I2C_ERR_GENERAL;

			*pBuffPtr = (uint8_t)I2C1->DR;
			pBuffPtr++;
			XferSize--;
			XferCount--;

			if(__HAL_I2C_GET_FLAG(HAL_I2C_FLAG_BTF) == SET)
			{
				if(XferSize == 3U) CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK); /* Disable Acknowledge */
				*pBuffPtr = (uint8_t)I2C1->DR;
				pBuffPtr++;
				XferSize--;
				XferCount--;
			}
		}
	}
	return 0;
}