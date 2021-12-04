#include "compassHandler.h"
#include <stdio.h>

volatile uint8_t rxBuffer[6];

void I2CSendData()
{
	I2CWriteCmd(0x01, 0x20);						// CRB_REG_M register to set guass scale: Set Sensor input	field range to +-1.3 guass
	I2CWriteCmd(0x02, 0x00);						// MR_REG_M register: 0* Continuous-conversion mode; 1 Single-conversion mode; 2 Sleep mode

	I2CWriteAddr(0x83);

	I2C1->CR2 |= 0x3C & I2C_CR2_SADD_Msk;
	I2C1->CR2 |= I2C_CR2_RD_WRN;					// 0*: Write transfer; 1: Read transfer

	// Read 6 bytes
	MODIFY_REG(I2C1->CR2, I2C_CR2_NBYTES_Msk, 6 << I2C_CR2_NBYTES_Pos);
	I2C1->CR2 |= I2C_CR2_START;						// Will continue sending address until ACK

	for (uint8_t i = 0; i < 6; ++i) {
		while ((I2C1->ISR & I2C_ISR_RXNE) == 0);	// Wait until data received
		rxBuffer[i] = I2C1->RXDR;					// Set register address
	}
	for (uint8_t i = 0; i < 6; ++i) {
		printf("Read data %d: 0x%X\r\n", i, rxBuffer[i]);		// Read data from OUT_X_H_M
	}



	volatile int x __attribute__((unused)) = 1;
}


// Read data from the selected register address
uint8_t I2CReadData()
{
	// 7-bit address (0x1E) plus 1 bit read/write identifier, i.e. 0x3D for read and 0x3C for write
//	I2C1->CR2 |= 0x3C & I2C_CR2_SADD_Msk;
//	I2C1->CR2 |= I2C_CR2_RD_WRN;				// 0*: Write transfer; 1: Read transfer
//	MODIFY_REG(I2C1->CR2, I2C_CR2_NBYTES_Msk, 1 << I2C_CR2_NBYTES_Pos);
	//I2C1->CR2 |= 1 << I2C_CR2_NBYTES_Pos;		// Receive 1 byte
	I2C1->CR2 |= I2C_CR2_START;					// Will continue sending address until ACK
	while ((I2C1->ISR & I2C_ISR_RXNE) == 0);	// Wait until data received
	return I2C1->RXDR;							// Set register address
}

// Sets the register address
void I2CWriteAddr(uint8_t reg)
{
	// 7-bit address (0x1E) plus 1 bit read/write identifier, i.e. 0x3D for read and 0x3C for write
	I2C1->CR2 |= 0x3C & I2C_CR2_SADD_Msk;
	I2C1->CR2 &= ~I2C_CR2_RD_WRN;				// 0*: Write transfer; 1: Read transfer
	MODIFY_REG(I2C1->CR2, I2C_CR2_NBYTES_Msk, 1 << I2C_CR2_NBYTES_Pos);		// Send 1 byte
	I2C1->CR2 |= I2C_CR2_START;					// Will continue sending address until ACK

	while ((I2C1->ISR & I2C_ISR_TXIS) == 0);	// Wait until ready
	I2C1->TXDR = reg;							// Set register address

}

// Writes data to a register
void I2CWriteCmd(uint8_t reg, uint8_t val)
{
	// 7-bit address (0x1E) plus 1 bit read/write identifier, i.e. 0x3D for read and 0x3C for write
	I2C1->CR2 |= 0x3C & I2C_CR2_SADD_Msk;
	I2C1->CR2 &= ~I2C_CR2_RD_WRN;				// 0*: Write transfer; 1: Read transfer
	MODIFY_REG(I2C1->CR2, I2C_CR2_NBYTES_Msk, 2 << I2C_CR2_NBYTES_Pos);		// Send 2 bytes
	I2C1->CR2 |= I2C_CR2_START;					// Will continue sending address until ACK

	while ((I2C1->ISR & I2C_ISR_TXIS) == 0);	// Wait until ready
	I2C1->TXDR = reg;							// Set register address
	while ((I2C1->ISR & I2C_ISR_TXIS) == 0);	// Wait until ready
	I2C1->TXDR = val;							// Write value to register

	while ((I2C1->ISR & I2C_ISR_TC) == 0);		// Wait until transmit complete
}
