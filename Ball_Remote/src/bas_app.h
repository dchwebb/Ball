#pragma once

struct BasService {
public:
	void Init();
	bool EventHandler(hci_event_pckt* Event);
	void SetLevel(uint8_t level);
private:
	uint16_t ServiceHandle;					// Service handle
	uint16_t BatteryLevelHandle;			// Characteristic handle
	uint16_t Level;							// Battery level
	bool     BatteryNotifications;			// Notifications enabled

	void AppInit();
	tBleStatus UpdateChar();
	static void SendNotification();
};

extern BasService basService;

