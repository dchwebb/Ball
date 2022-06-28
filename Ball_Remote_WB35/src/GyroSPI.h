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
private:
	bool multipleRead {false};

	void StartRead();
};

extern GyroSPI gyro;
