#pragma once

#include "tl.h"

struct BasService {
public:
	void Init();
	bool EventHandler(hci_event_pckt* Event);
	float GetBatteryLevel();
	void TimedRead();
	uint32_t SerialiseConfig(uint8_t** buff);
	uint32_t StoreConfig(uint8_t* buff);

	uint16_t level;							// Battery level as a percentage

	// Settings that can be saved to flash
	struct Settings {
		uint16_t shutdownLevel;				// Shutdown when Battery level percentage fall too low
	} settings {5};

private:
	uint16_t serviceHandle;					// Service handle
	uint16_t batteryLevelHandle;			// Characteristic handle
	bool     batteryNotifications;			// Notifications enabled
	uint32_t lastRead = 0;					// Last systick time battery level was read
	uint32_t lastSent = 0;					// Last systick time battery level was sent

	static void UpdateBatteryChar();
};

extern BasService basService;

