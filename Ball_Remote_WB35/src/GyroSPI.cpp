#include "gyroSPI.h"
#include "hids_app.h"
#include <stdio.h>

GyroSPI gyro;		// For use with ST L3GD20


void GyroSPI::Setup()
{
	WriteCmd(0x20, 0x6F);									// CTRL_REG1: DR = 01 (200 Hz ODR); BW = 10 (50 Hz bandwidth); PD = 1 (normal mode); Zen = Yen = Xen = 1 (all axes enabled)

	// Configure read settings
	SPI1->CR2 &= ~SPI_CR2_DS;								// Set data size to 8 bit
	SPI1->CR2 |= SPI_CR2_FRXTH;								// Set FIFO threshold to 8 bits
}


// Writes data to a register
void GyroSPI::WriteCmd(uint8_t reg, uint8_t val)
{
	// 16 bit command format: ~RW | ~MS | AD5 | ... | AD0 | DI7 | ... | DI0
	// MS bit: 0 = address remains unchanged in multiple read/write commands

	SPI1->CR2 |= SPI_CR2_DS;								// Set data size to 16 bit
	GPIOA->ODR &= ~GPIO_ODR_OD15;							// Set CS low

	SPI1->DR = (reg << 8) | val;
	while ((SPI1->SR & SPI_SR_BSY) != 0) {}

	GPIOA->ODR |= GPIO_ODR_OD15;							// Set CS high
	SPI1->CR2 &= ~SPI_CR2_DS;								// Set data size to 8 bit
}


uint8_t GyroSPI::ReadRegister(uint8_t reg)
{
	GPIOA->ODR &= ~GPIO_ODR_OD15;							// Set CS low

	*spi8BitWrite = readGyro | reg; 						// set RW bit to 1 to indicate read
	ClearReadBuffer();										// Clear RX buffer while data is sending
	*spi8BitWrite = 0;										// Dummy write to trigger read - add to FIFO

	while ((SPI1->SR & SPI_SR_RXNE) == 0) {}
	[[maybe_unused]] volatile uint8_t dummy = SPI1->DR;		// Clear dummy read

	while ((SPI1->SR & SPI_SR_RXNE) == 0) {}				// Wait for RX data to be ready
	GPIOA->ODR |= GPIO_ODR_OD15;							// Set CS high

	return SPI1->DR;
}


void GyroSPI::GyroRead()
{
	// Read x/y/z registers continuously - after first command to read and increment address, each byte read is triggered with a byte write
	uint8_t gyroBuffer[6];

	GPIOA->ODR &= ~GPIO_ODR_OD15;							// Set CS low
	*spi8BitWrite = readGyro | incrAddr | dataRegStart;		// Send instruction to trigger reads
	ClearReadBuffer();										// Clear RX buffer while data is sending
	*spi8BitWrite = 0;										// Add dummy write to FIFO to trigger first read

	while ((SPI1->SR & SPI_SR_RXNE) == 0) {}
	[[maybe_unused]] volatile uint8_t dummy = SPI1->DR;		// Clear dummy read

	for (uint8_t i = 0; i < 6; ++i) {
		*spi8BitWrite = 0;									// Dummy write to trigger next read
		while ((SPI1->SR & SPI_SR_RXNE) == 0) {}
		gyroBuffer[i] = SPI1->DR;
	}

	GPIOA->ODR |= GPIO_ODR_OD15;							// Set CS high

	uint16_t* buff16bit = (uint16_t*)gyroBuffer;
	gyroData.x = buff16bit[0];
	gyroData.y = buff16bit[1];
	gyroData.z = buff16bit[2];
}


void GyroSPI::OutputGyro()
{
	GyroRead();
	hidService.JoystickNotification(gyroData.x, gyroData.y, gyroData.z);
}


void GyroSPI::ContinualRead(bool on)
{
	// Initiate Timer to output continual readings
	if (on) {
		TIM2->CR1 |= TIM_CR1_CEN;
		NVIC_EnableIRQ(TIM2_IRQn);
	} else {
		TIM2->CR1 &= ~TIM_CR1_CEN;
	}
}
