#include <HidService.h>
#include "gyroSPI.h"
#include <stdio.h>

GyroSPI gyro;		// For use with ST L3GD20


void GyroSPI::Setup()
{
	// Configure wake up on movement registers
	WriteCmd(0x30, 0x2A);									// INT1_CFG: Enable Interrupts on X/Y/Z high
	WriteCmd(0x32, 0x07);									// INT1_THS_XH: Set X high threshold to 1792
	WriteCmd(0x34, 0x07);									// INT1_THS_YH: Set Y high threshold to 1792
	WriteCmd(0x36, 0x07);									// INT1_THS_ZH: Set Z high threshold to 1792

	// Configure read settings
	SPI1->CR2 &= ~SPI_CR2_DS;								// Set data size to 8 bit
	SPI1->CR2 |= SPI_CR2_FRXTH;								// Set FIFO threshold to 8 bits
}



void GyroSPI::Configure(SetConfig mode)
{
	switch (mode) {
	case SetConfig::PowerDown:
		WriteCmd(0x22, 0x00);								// CTRL_REG3: Disable Gyroscope Interrupt and data ready output pins
		WriteCmd(0x20, 0x00);								// CTRL_REG1: Power down
		break;
	case SetConfig::WakeupInterrupt:						// Called before sleeping to wake on gyro movement
		WriteCmd(0x22, 0x80);								// CTRL_REG3: Enable Gyroscope Interrupt output pin for wakeup
		break;
	case SetConfig::ContinousOutput:
		WriteCmd(0x20, 0x8F);								// CTRL_REG1: DR = 10 (380 Hz ODR); BW = 00 (20 Hz bandwidth); PD = 1 (normal mode); Zen = Yen = Xen = 1 (all axes enabled)
		WriteCmd(0x22, 0x08);								// CTRL_REG3: Output data ready on RDY pin
		GyroRead();											// Force read to clear interrupt
		break;
	}
}


void GyroSPI::SamplingSpeed(Sampling speed)
{
	if (speed != currentSpeed) {
		if (speed == Sampling::Slow) {
			WriteCmd(0x20, 0x4F);							// CTRL_REG1: DR = 10 (380 Hz ODR); BW = 00 (20 Hz bandwidth); PD = 1 (normal mode); Zen = Yen = Xen = 1 (all axes enabled)
		} else {
			WriteCmd(0x20, 0x8F);							// CTRL_REG1: DR = 01 (190 Hz ODR); BW = 00 (20 Hz bandwidth); PD = 1 (normal mode); Zen = Yen = Xen = 1 (all axes enabled)
		}
		currentSpeed = speed;
	}
}


// Writes data to a register
void GyroSPI::WriteCmd(uint8_t reg, uint8_t val)
{
	// 16 bit command format: ~RW | ~MS | AD5 | ... | AD0 | DI7 | ... | DI0
	// MS bit: 0 = address remains unchanged in multiple read/write commands
	if (!busy) {
		busy =  true;
		SPI1->CR2 |= SPI_CR2_DS;							// Set data size to 16 bit
		GPIOA->ODR &= ~GPIO_ODR_OD15;						// Set CS low

		SPI1->DR = (reg << 8) | val;
		while ((SPI1->SR & SPI_SR_BSY) != 0) {}

		GPIOA->ODR |= GPIO_ODR_OD15;						// Set CS high
		SPI1->CR2 &= ~SPI_CR2_DS;							// Set data size to 8 bit
		busy =  false;
	}
}


uint8_t GyroSPI::ReadRegister(uint8_t reg)
{
	if (!busy) {
		busy =  true;
		GPIOA->ODR &= ~GPIO_ODR_OD15;						// Set CS low

		*spi8BitWrite = readGyro | reg; 					// set RW bit to 1 to indicate read
		ClearReadBuffer();									// Clear RX buffer while data is sending
		*spi8BitWrite = 0;									// Dummy write to trigger read - add to FIFO

		while ((SPI1->SR & SPI_SR_RXNE) == 0) {}
		[[maybe_unused]] volatile uint8_t dummy = SPI1->DR;	// Clear dummy read

		while ((SPI1->SR & SPI_SR_RXNE) == 0) {}			// Wait for RX data to be ready
		GPIOA->ODR |= GPIO_ODR_OD15;						// Set CS high

		busy =  false;
	}
	return SPI1->DR;
}


void GyroSPI::GyroRead()
{
	// Read x/y/z registers continuously - after first command to read and increment address, each byte read is triggered with a byte write
	if (!busy) {
		busy =  true;
		uint8_t gyroBuffer[6];

		GPIOA->ODR &= ~GPIO_ODR_OD15;						// Set CS low
		*spi8BitWrite = readGyro | incrAddr | dataRegStart;	// Send instruction to trigger reads
		ClearReadBuffer();									// Clear RX buffer while data is sending
		*spi8BitWrite = 0;									// Add dummy write to FIFO to trigger first read

		while ((SPI1->SR & SPI_SR_RXNE) == 0) {}
		[[maybe_unused]] volatile uint8_t dummy = SPI1->DR;	// Clear dummy read

		for (uint8_t i = 0; i < 6; ++i) {
			*spi8BitWrite = 0;								// Dummy write to trigger next read
			while ((SPI1->SR & SPI_SR_RXNE) == 0) {}
			gyroBuffer[i] = SPI1->DR;
		}

		GPIOA->ODR |= GPIO_ODR_OD15;						// Set CS high
		busy =  false;

		uint16_t* buff16bit = (uint16_t*)gyroBuffer;
		gyroData.x = buff16bit[0];
		gyroData.y = buff16bit[1];
		gyroData.z = buff16bit[2];
	}
}


void GyroSPI::OutputGyro()
{
	GyroRead();
	hidService.JoystickNotification(gyroData.x, gyroData.y, gyroData.z);
}


