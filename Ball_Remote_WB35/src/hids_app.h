#pragma once

#include "ble_legacy.h"

struct HidService
{
public:
	bool JoystickNotifications;
	bool outputGyro {false};

	void Init();
	void JoystickNotification(int16_t x, int16_t y, int16_t z);
	void Disconnect();
	bool EventHandler(hci_event_pckt* Event);
private:
	enum HIDInfoFlags {RemoteWake = 1, NormallyConnectable = 2};
	enum Characteristic {ReportMap, HidInformation, ReportJoystick};
	enum CharOffset {ValueOffset = 1, DescriptorOffset = 2};

	static constexpr uint16_t JoystickReportID {0x0101};	// First Byte: Input Report (0x01) | Second Byte: Report ID (0x1 ... 0x03)

	const struct HIDInformation {
		uint16_t bcdHID {0x0101};			// Binary coded decimal HID version
		uint8_t  bcountryCode {0};  		// 0 = not localized
		uint8_t  flags = {RemoteWake | NormallyConnectable};          // See http://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.hid_information.xml
	} hidInformation;

	struct {uint16_t x; uint16_t y; uint16_t z;} joystickReport;

	uint16_t ServiceHandle;				 	// Service handle
	uint16_t ReportJoystickHandle;
	uint16_t ReportJoystickRefDescHandle;
	uint16_t HidInformationHandle;
	uint16_t HidControlPointHdle;
	uint16_t ReportMapHandle;

	void AppInit();
	void UpdateJoystickReportChar();
	void UpdateReportMapChar();
	void UpdateHidInformationChar();
	void ControlPointWrite(uint16_t data);
};


// Joystick HID map with 16 bit x, y, z controls
static constexpr uint8_t reportMap[] = {
		// Header codes are formed of: tag (4 bits), type (2 bits), size (2 bits) [see USB_hid1_11 6.2.2.7]
		// Eg logical maximum 0x26 = 0010 01 10 = tag: 2 (logical maximum), type: 1 (global), size: 2 bytes
		0x05, 0x01,			// Usage Page (Generic Desktop)
		0x09, 0x04,			// Usage (Joystick)
		0xA1, 0x01,			// Collection (Application)
		0xA1, 0x02,				// Collection (Logical)

		0x75, 0x10,					// Report size (16)
		0x95, 0x03,					// Report count (3)
		0x15, 0x00,					// Logical Minimum (0) [Logical = actual data being sent]
		0x26, 0xFF, 0x0F,			// Logical Maximum (4095)
		0x35, 0x00,					// Physical Minumum (0) [Physical = maps logical values to real unit values]
		0x46, 0xFF, 0x0F,			// Physical Maximum (4095)
		0x09, 0x30,					// Usage (X)
		0x09, 0x31,					// Usage (Y)
		0x09, 0x32,					// Usage (Z)
		0x81, 0x02,					// Input (Data, Variable, Absolute)

		0xC0,					// End collection
		0xC0				// End collection
};

extern HidService hidService;
