#pragma once

#include "initialisation.h"

struct DisService {
public:
	void Init();
private:
	static constexpr uint8_t DIS_Name[9] = "MOUNTJOY";

	uint16_t DevInfoSvcHdle;       			// Service handle
	uint16_t ManufacturerNameStringCharHdle; // Characteristic handle
};

extern DisService disService;
