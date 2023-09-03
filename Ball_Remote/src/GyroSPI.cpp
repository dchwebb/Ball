#include "gyroSPI.h"
#include "hids_app.h"
#include <stdio.h>

GyroSPI gyro;

// For use with ST L3GD20
// PB8: I2C1_SCL; PB9: I2C1_SDA

// The I2C register address should cycle from 3 to 8 for sequential reads from the gyro registers;
// However there seems to be an issue as currently configured where after reading 6 registers the pointer jumps an extra place
// Botch fix is to read 11 times - this will then wrap to the next start point without having to reset registers
#define I2CCOUNT 11
volatile int8_t gyroBuffer[I2CCOUNT];

void GyroSPI::Setup()
{
	//GPIOA->ODR &= ~GPIO_ODR_OD15;					// Set CS low
	WriteCmd(0x20, 0x6F);							// CTRL_REG1: DR = 01 (200 Hz ODR); BW = 10 (50 Hz bandwidth); PD = 1 (normal mode); Zen = Yen = Xen = 1 (all axes enabled)
	//GPIOA->ODR |= GPIO_ODR_OD15;					// Set CS high
}

// Writes data to a register
void GyroSPI::WriteCmd(uint8_t reg, uint8_t val)
{
	// 16 bit command format: ~RW | ~MS | AD5 | ... | AD0 | DI7 | ... | DI0
	// MS bit: 0 = address remains unchanged in multiple read/write commands
	uint16_t cmd = (reg << 8) | val;
	SPI1->CR2 |= SPI_CR2_DS;						// Set data size to 16 bit
	SPI1->DR = cmd;
}

void GyroSPI::DebugRead() {
	// Triggers a one-off read of gyro x, y and z registers for debugging
	const uint8_t regStart = 0x28;
	uint8_t gyroBuffer[6];
	//SPI1->CR2 |= (16 << SPI_CR2_DS_Pos);				// Set data size to 16 bit

	for (uint8_t i = 0; i < 6; ++i) {
		//GPIOA->ODR &= ~GPIO_ODR_OD15;					// Set CS low
		uint16_t cmd  = (1 << 15) | ((regStart + i) << 8); 					// set RW bit to 1 to indicate read
		SPI1->DR = cmd;
		while ((SPI1->SR & SPI_SR_RXNE) == 0) {}
		//GPIOA->ODR |= GPIO_ODR_OD15;					// Set CS high
		gyroBuffer[i] = SPI1->DR;
	}

	//SPI1->CR2 |= (8 << SPI_CR2_DS_Pos);				// Set data size to 8 bit
	gyroData.x = (gyroBuffer[0] << 8) | gyroBuffer[1];
	gyroData.y = (gyroBuffer[2] << 8) | gyroBuffer[3];
	gyroData.z = (gyroBuffer[4] << 8) | gyroBuffer[5];
}

// Read data from the selected register address
uint8_t GyroSPI::ReadData(uint8_t reg)
{
	// This pulses the CS pin after 8 bits - possibly use 16 bit format and see if data is returned in second half of word
	while ((SPI1->SR & SPI_SR_RXNE) == SPI_SR_RXNE) {
		[[maybe_unused]] volatile uint16_t dummy = SPI1->DR;
	}

#ifdef SPI_8BIT
	SPI1->CR2 &= ~SPI_CR2_DS_3;						// Set data size to 8 bit
	uint8_t cmd  = (1 << 7) | reg; 					// set RW bit to 1 to indicate read
	SPI1->DR = cmd;
	while ((SPI1->SR & SPI_SR_RXNE) == 0) {}
	return SPI1->DR;
#else
	uint16_t cmd  = (1 << 15) | (reg << 8); 					// set RW bit to 1 to indicate read
	SPI1->DR = cmd;
	while ((SPI1->SR & SPI_SR_RXNE) == 0) {}
	return SPI1->DR;

#endif
}


void GyroSPI::ContinualRead()
{

}
