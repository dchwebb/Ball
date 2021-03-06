#pragma once

#include "initialisation.h"

struct Gyro {
public:
	static constexpr uint8_t i2cAddress = 0xD2;

	void Setup();
	void ProcessResults();
	uint32_t ScanAddresses();
	uint8_t ReadData(uint8_t reg);
	void WriteCmd(uint8_t reg, uint8_t val);
	void DebugRead();
	void ContinualRead();
private:
	bool multipleRead {false};

	void WriteAddr(uint8_t reg);
	void StartRead();
};

extern Gyro gyro;
