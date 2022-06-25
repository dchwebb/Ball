#include "gyroHandler.h"
#include "hids_app.h"
#include <stdio.h>

Gyro gyro;

// For use with ST L3GD20
// PB8: I2C1_SCL; PB9: I2C1_SDA

// The I2C register address should cycle from 3 to 8 for sequential reads from the gyro registers;
// However there seems to be an issue as currently configured where after reading 6 registers the pointer jumps an extra place
// Botch fix is to read 11 times - this will then wrap to the next start point without having to reset registers
#define I2CCOUNT 11
volatile int8_t gyroBuffer[I2CCOUNT];

void Gyro::Setup()
{
	DMA1_Channel1->CMAR = (uint32_t)&gyroBuffer[0];
	DMA1_Channel1->CPAR = (uint32_t)&(I2C1->RXDR);
	DMA1_Channel1->CCR |= DMA_CCR_CIRC;				// Avoids having to continually alter the data count
	DMA1_Channel1->CNDTR = I2CCOUNT;				// DMA Read count in bytes
	DMA1_Channel1->CCR |= DMA_CCR_EN;				// Enable peripheral

	MODIFY_REG(I2C1->CR2, I2C_CR2_SADD_Msk, Gyro::i2cAddress);
	WriteCmd(0x20, 0x6F);							// CTRL_REG1: DR = 01 (200 Hz ODR); BW = 10 (50 Hz bandwidth); PD = 1 (normal mode); Zen = Yen = Xen = 1 (all axes enabled)
}


void Gyro::DebugRead() {
	// Triggers a one-off read of gyro x, y and z registers for debugging
	multipleRead = true;
	ContinualRead();
}


void Gyro::ContinualRead()
{
	WriteAddr(0xA8);								// MSB = 1 to indicate multiple reads; 0x28 = First read address: OUT_X_L (p.23)

	// Configure for repeated reads
	I2C1->CR2 |= I2C_CR2_STOP;						// Clear interrupts - will be reset when next START is issued
	I2C1->CR2 |= I2C_CR2_RD_WRN;					// 0: Write transfer; 1*: Read transfer
	I2C1->CR1 |= I2C_CR1_RXDMAEN;					// Enable DMA transmission
	I2C1->CR1 |= I2C_CR1_TCIE;						// Activate Transfer complete interrupt
	MODIFY_REG(I2C1->CR2, I2C_CR2_NBYTES_Msk, I2CCOUNT << I2C_CR2_NBYTES_Pos);		// I2C Read count in bytes

	StartRead();
}


void Gyro::StartRead()
{
	I2C1->CR2 |= I2C_CR2_START;
}


void Gyro::ProcessResults()
{
	if (multipleRead) {
		multipleRead = false;
		printf("x: %d, y:%d, z: %d\n",
				(gyroBuffer[1] << 8) | gyroBuffer[0],
				(gyroBuffer[3] << 8) | gyroBuffer[2],
				(gyroBuffer[5] << 8) | gyroBuffer[4]);

	} else {
		hidService.JoystickNotification(
				(gyroBuffer[1] << 8) | gyroBuffer[0],
				(gyroBuffer[3] << 8) | gyroBuffer[2],
				(gyroBuffer[5] << 8) | gyroBuffer[4]
				);

		if (hidService.JoystickNotifications) {
			StartRead();
		}
	}
}


// Sets the register address
void Gyro::WriteAddr(uint8_t reg)
{
	uint32_t timer = 0;

	I2C1->CR1 &= ~I2C_CR1_TCIE;						// Disable Transfer complete interrupt (as this clears TC flag)
	I2C1->CR1 &= ~I2C_CR1_RXDMAEN;					// Disable DMA transmission

	I2C1->CR2 &= ~I2C_CR2_RD_WRN;					// 0*: Write transfer; 1: Read transfer
	MODIFY_REG(I2C1->CR2, I2C_CR2_NBYTES_Msk, 1 << I2C_CR2_NBYTES_Pos);		// Send 1 byte
	I2C1->CR2 |= I2C_CR2_START;						// Will continue sending address until ACK

	while ((I2C1->ISR & I2C_ISR_TXIS) == 0 && ++timer < 10000);		// Wait until ready
	I2C1->TXDR = reg;								// Set register address

	timer = 0;
	while ((I2C1->ISR & I2C_ISR_TC) == 0 && ++timer < 10000);			// Wait until transmit complete
//	I2C1->CR1 |= I2C_CR1_TCIE;						// Activate Transfer complete interrupt
}


// Writes data to a register
void Gyro::WriteCmd(uint8_t reg, uint8_t val)
{
	I2C1->CR1 &= ~I2C_CR1_TCIE;						// Disable Transfer complete interrupt (as this clears TC flag)

	I2C1->CR2 &= ~I2C_CR2_RD_WRN;					// 0*: Write transfer; 1: Read transfer
	MODIFY_REG(I2C1->CR2, I2C_CR2_NBYTES_Msk, 2 << I2C_CR2_NBYTES_Pos);		// Send 2 bytes
	I2C1->CR2 |= I2C_CR2_START;						// Will continue sending address until ACK

	while ((I2C1->ISR & I2C_ISR_TXIS) == 0);		// Wait until ready
	I2C1->TXDR = reg;								// Set register address
	while ((I2C1->ISR & I2C_ISR_TXIS) == 0);		// Wait until ready
	I2C1->TXDR = val;								// Write value to register

	while ((I2C1->ISR & I2C_ISR_TC) == 0);			// Wait until transmit complete

//	I2C1->CR1 |= I2C_CR1_TCIE;						// Activate Transfer complete interrupt
//	I2C1->CR2 |= I2C_CR2_RD_WRN;					// 0: Write transfer; 1*: Read transfer
}


// Read data from the selected register address
uint8_t Gyro::ReadData(uint8_t reg)
{
	I2C1->CR1 &= ~I2C_CR1_RXDMAEN;					// Disable DMA transmission
	MODIFY_REG(I2C1->CR2, I2C_CR2_NBYTES_Msk, 1 << I2C_CR2_NBYTES_Pos);		// Receive 1 byte
	//I2C1->CR1 &= ~I2C_CR1_TCIE;						// Disable Transfer complete interrupt

	WriteAddr(reg);									// set read address

	I2C1->CR2 |= I2C_CR2_RD_WRN;					// 0: Write transfer; 1*: Read transfer
	I2C1->CR2 |= I2C_CR2_START;						// Will continue sending address until ACK
	while ((I2C1->ISR & I2C_ISR_RXNE) == 0);		// Wait until data received
	uint8_t ret = I2C1->RXDR;						// Set register address

//	Setup();										// Reset default values

	return ret;
}


uint32_t Gyro::ScanAddresses()
{
	// Scan for legitimate I2C addresses
	uint32_t found = 0;

	I2C1->CR2 &= ~I2C_CR2_RD_WRN;					// 0*: Write transfer; 1: Read transfer
	MODIFY_REG(I2C1->CR2, I2C_CR2_NBYTES_Msk, 1 << I2C_CR2_NBYTES_Pos);		// Send 1 byte

	for (uint8_t addr = 0x00; addr < 0xFF; ++addr) {
		I2C1->CR1 &= ~I2C_CR1_PE;					// Disable peripheral - otherwise interrupts do not clear correctly
		MODIFY_REG(I2C1->CR2, I2C_CR2_SADD_Msk, addr);
		I2C1->CR1 |= I2C_CR1_PE;					// Enable peripheral
		I2C1->CR2 |= I2C_CR2_START;					// Will continue sending address until ACK

		uint32_t start = uwTick;
		while ((I2C1->ISR & I2C_ISR_TXIS) == 0 && (uwTick < start + 50));		// Wait until ready
		if ((I2C1->ISR & I2C_ISR_TXIS) == I2C_ISR_TXIS) {
			printf("Found: 0x%X\n", addr);
			found++;
		}
	}

	I2C1->CR1 &= ~I2C_CR1_PE;						// Disable peripheral - otherwise interrupts do not clear correctly
	MODIFY_REG(I2C1->CR2, I2C_CR2_SADD_Msk, Gyro::i2cAddress);
	I2C1->CR1 |= I2C_CR1_PE;						// Enable peripheral

	return found;
}
