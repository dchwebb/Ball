#pragma once

#include "stdint.h"
#include "svc_ctl.h"


struct HidApp {
public:
	enum class HidState {Idle, ClientConnected, Disconnect, DiscoverServices, DiscoverCharacteristics, DiscoveredCharacteristics, DiscoveringCharacteristics, EnableNotificationDesc, EnableHIDNotificationDesc};
	HidState state;								// state of the HID Client state machine

	enum class HidAction {None, Connect, GetReportMap, BatteryLevel};
	HidAction action;

	struct Position3D {
		int16_t x{2047};
		int16_t y{2047};
		int16_t z{2047};
	} position3D;

	float divider = 600.0f;						// Divider for raw gyroscope data (increase for more sensitivity)
	float offsetX{-128.0f};						// Offsets for raw incoming hid data
	float offsetY{-100.0f};
	float offsetZ{-76.0f};
	bool outputGyro{false};						// Set to true to output raw gyro and received

	void Init(void);
	void HIDConnectionNotification();
	void Calibrate();
	void GetReportMap();
	static void GetBatteryLevel();

private:
	uint16_t connHandle;						// connection handle

	uint16_t HIDServiceHandle;					// handle of the HID service
	uint16_t HIDServiceEndHandle;				// end handle of the HID service
	uint16_t HIDNotificationCharHdle;			// handle of the HID Report characteristic
	uint16_t HIDNotificationDescHandle;			// handle of the HID report client configuration descriptor
	uint16_t BatteryServiceHandle;				// handle of the Battery service
	uint16_t BatteryServiceEndHandle;			// end handle of the Battery service
	uint16_t BatteryNotificationCharHdle;		// handle of the Rx characteristic - Notification From Server
	uint16_t BatteryNotificationDescHandle;		// handle of the client configuration descriptor of Rx characteristic
	uint16_t HIDReportMapHdle;					// handle of report map

	int32_t calibrateCounter{0};				// When calibrating offsets keeps count of readings averaged
	static constexpr int32_t calibrateCount{200};				// Total number of readings to use when calibrating
	float calibX, calibY, calibZ;
	//Position3D calibDebug[calibrateCount];

	static SVCCTL_EvtAckStatus_t HIDEventHandler(void *Event);
	static void HIDServiceDiscovery();
	void HidNotification(uint8_t* payload, uint8_t len);
	void BatteryNotification(uint8_t* payload, uint8_t len);
	void PrintReportMap(uint8_t* data, uint8_t len);
};

extern HidApp hidApp;

