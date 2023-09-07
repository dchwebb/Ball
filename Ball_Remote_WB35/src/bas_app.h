#pragma once

struct BasService {
public:
	void Init();
	bool EventHandler(hci_event_pckt* Event);
	float GetBatteryLevel();
	void TimedRead();
private:
	uint16_t ServiceHandle;					// Service handle
	uint16_t BatteryLevelHandle;			// Characteristic handle
	uint16_t Level;							// Battery level
	bool     BatteryNotifications;			// Notifications enabled
	uint32_t lastRead = 0;					// Last systick time battery level was read

	void AppInit();
	static void UpdateBatteryChar();
};

extern BasService basService;

