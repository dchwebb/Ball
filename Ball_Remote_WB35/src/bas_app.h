#pragma once

#include "tl.h"

struct BasService {
public:
	void Init();
	bool EventHandler(hci_event_pckt* Event);
	float GetBatteryLevel();
	void TimedRead();

	uint16_t Level;							// Battery level as a percentage

private:
	uint16_t ServiceHandle;					// Service handle
	uint16_t BatteryLevelHandle;			// Characteristic handle
	bool     BatteryNotifications;			// Notifications enabled
	uint32_t lastRead = 0;					// Last systick time battery level was read
	uint32_t lastSent = 0;					// Last systick time battery level was sent

	void AppInit();
	static void UpdateBatteryChar();
};

extern BasService basService;

