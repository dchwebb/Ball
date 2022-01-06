#pragma once

#include "stdint.h"
#include "svc_ctl.h"


struct HidApp {
public:
	enum class HidState {Idle, ClientConnected, DiscoverServices, DiscoverCharacs, DiscoverNotificationCharDesc, DiscoveringCharacs, EnableNotificationDesc, EnableHIDNotificationDesc};
	HidState state;								// state of the HID Client state machine
	struct Position3D {
		int16_t x;
		int16_t y;
		int16_t z;
	} position3D;

	void Init(void);
	void HIDConnectionNotification();

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

	static SVCCTL_EvtAckStatus_t HIDEventHandler(void *Event);
	static void UpdateService();
	void HidNotification(uint8_t* payload, uint8_t len);
	void BatteryNotification(uint8_t* payload, uint8_t len);
	HidState GetState();
};

void HID_APP_SW1_Button_Action();



