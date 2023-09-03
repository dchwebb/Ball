#pragma once

#include "initialisation.h"

struct GyroSPI {
public:
	void Setup();
	void ProcessResults();
	uint8_t ReadData(uint8_t reg);
	void WriteCmd(uint8_t reg, uint8_t val);
	void DebugRead();
	void ContinualRead();

	struct {
		int16_t x;
		int16_t y;
		int16_t z;
	} gyroData;

private:
	bool multipleRead {false};

	void StartRead();
};

extern GyroSPI gyro;
