#pragma once

#include "initialisation.h"

struct Compass {
	void Setup();
	void StartRead();
	void WriteCmd(uint8_t reg, uint8_t val);
	void WriteAddr(uint8_t reg);
	uint8_t ReadData(uint8_t reg);
	void ProcessResults();
};

extern Compass compass;
