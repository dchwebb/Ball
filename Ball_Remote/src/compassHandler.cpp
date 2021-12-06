#include "compassHandler.h"
#include <stdio.h>

// Fixme - the I2C register address should cycle from 3 to 8 for sequential reads from the compass registers;
// However there seems to be an issue as currently configured where after reading 6 registers the pointer jumps an extra place
// Botch fix is to read 11 times - this will then wrap to the next start point without having to reset registers
#define I2CCOUNT 11
volatile int8_t i2cBuffer[I2CCOUNT];
struct  {
	int16_t x;
	int16_t y;
	int16_t z;
} compassData;

bool compassCapture = false;

void I2CSetup()
{
	I2C1->CR2 |= 0x3C & I2C_CR2_SADD_Msk;
	I2CWriteCmd(0x01, 0x20);						// CRB_REG_M register to set guass scale: Set Sensor input	field range to +-1.3 guass
	I2CWriteCmd(0x02, 0x00);						// MR_REG_M register: 0* Continuous-conversion mode; 1 Single-conversion mode; 2 Sleep mode
	I2CWriteAddr(0x83);								// MSB = 1 to indicate multiple reads; 0x03 = First manget read address: OUT_X_H_M

	// Configure for repeated reads
	I2C1->CR2 |= I2C_CR2_STOP;						// Clear interrupts - will be reset when next START is issued
	I2C1->CR2 |= I2C_CR2_RD_WRN;					// 0: Write transfer; 1*: Read transfer
	I2C1->CR1 |= I2C_CR1_RXDMAEN;					// Enable DMA transmission
	I2C1->CR1 |= I2C_CR1_TCIE;						// Activate Transfer complete interrupt
	MODIFY_REG(I2C1->CR2, I2C_CR2_NBYTES_Msk, I2CCOUNT << I2C_CR2_NBYTES_Pos);		// I2C Read count in bytes

	DMA1_Channel1->CMAR = (uint32_t)&i2cBuffer[0];
	DMA1_Channel1->CPAR = (uint32_t)&(I2C1->RXDR);
	DMA1_Channel1->CCR |= DMA_CCR_CIRC;				// Avoids having to continually alter the data count
	DMA1_Channel1->CNDTR = I2CCOUNT;				// DMA Read count in bytes
	DMA1_Channel1->CCR |= DMA_CCR_EN;				// Enable peripheral
}


void I2CStartRead()
{
	I2C1->CR2 |= I2C_CR2_START;
}


void I2CProcessResults()
{
	static uint32_t lastPrint = 0;

	compassData.x = (i2cBuffer[0] << 8) | i2cBuffer[1];
	compassData.y = (i2cBuffer[2] << 8) | i2cBuffer[3];
	compassData.z = (i2cBuffer[4] << 8) | i2cBuffer[5];

	if (uwTick - lastPrint > 200) {
		printf("x: %d y: %d z: %d\r\n", compassData.x, compassData.y, compassData.z);
		lastPrint = uwTick;
	}

	if (compassCapture) {
		I2CStartRead();
	}
}


// Sets the register address
void I2CWriteAddr(uint8_t reg)
{
	// 7-bit address (0x1E) plus 1 bit read/write identifier, i.e. 0x3D for read and 0x3C for write
	I2C1->CR2 &= ~I2C_CR2_RD_WRN;				// 0*: Write transfer; 1: Read transfer
	MODIFY_REG(I2C1->CR2, I2C_CR2_NBYTES_Msk, 1 << I2C_CR2_NBYTES_Pos);		// Send 1 byte
	I2C1->CR2 |= I2C_CR2_START;					// Will continue sending address until ACK

	while ((I2C1->ISR & I2C_ISR_TXIS) == 0);	// Wait until ready
	I2C1->TXDR = reg;							// Set register address

	while ((I2C1->ISR & I2C_ISR_TC) == 0);		// Wait until transmit complete
}

// Writes data to a register
void I2CWriteCmd(uint8_t reg, uint8_t val)
{
	// 7-bit address (0x1E) plus 1 bit read/write identifier, i.e. 0x3D for read and 0x3C for write
	I2C1->CR2 &= ~I2C_CR2_RD_WRN;				// 0*: Write transfer; 1: Read transfer
	MODIFY_REG(I2C1->CR2, I2C_CR2_NBYTES_Msk, 2 << I2C_CR2_NBYTES_Pos);		// Send 2 bytes
	I2C1->CR2 |= I2C_CR2_START;					// Will continue sending address until ACK

	while ((I2C1->ISR & I2C_ISR_TXIS) == 0);	// Wait until ready
	I2C1->TXDR = reg;							// Set register address
	while ((I2C1->ISR & I2C_ISR_TXIS) == 0);	// Wait until ready
	I2C1->TXDR = val;							// Write value to register

	while ((I2C1->ISR & I2C_ISR_TC) == 0);		// Wait until transmit complete
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
