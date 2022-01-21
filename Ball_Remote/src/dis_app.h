#pragma once

struct DevInfoService {
public:
	void Init();
	void AppInit();
private:
	static constexpr uint8_t DIS_Name[9] = "MOUNTJOY";

	uint16_t DevInfoSvcHdle;       // Service handle
	uint16_t ManufacturerNameStringCharHdle; // Characteristic handle
};

extern "C" {		// Declare with C linkage or will be overridden by weak declaration in svc_ctl.c
void DIS_Init();
}
extern DevInfoService devInfoService;
