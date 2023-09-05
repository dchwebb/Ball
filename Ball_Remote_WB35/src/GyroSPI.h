#pragma once

#include "initialisation.h"

struct GyroSPI {
public:
	void Setup();
	uint8_t ReadRegister(uint8_t reg);
	void WriteCmd(uint8_t reg, uint8_t val);
	void GyroRead();							// Reads Gyro x/y/z values
	void OutputGyro();							// Reads Gyro values and outputs via HID
	void ContinualRead(bool on);				// Starts timer to output continual Gyro readings

	struct {
		int16_t x;
		int16_t y;
		int16_t z;
	} gyroData;

private:
	static constexpr uint16_t readGyro = (1 << 7);
	static constexpr uint16_t incrAddr = (1 << 6);
	static constexpr uint16_t dataRegStart = 0x28;		// Address of gyroscope register holding first data byte in sequence for full read
	uint8_t* const spi8BitWrite = (uint8_t*)&SPI1->DR;

	inline void ClearReadBuffer()
	{
		while ((SPI1->SR & SPI_SR_RXNE) == SPI_SR_RXNE) {
			[[maybe_unused]] volatile uint16_t dummy = SPI1->DR;
		}
	}
};

extern GyroSPI gyro;
