#pragma once

#include "tl.h"

struct HidService
{
public:
	bool JoystickNotifications;
	bool outputGyro {false};
	uint8_t moving = 0xFF;							// Bit array - each bit shows whether one set of 256 measurements had movement
	uint32_t lastMovement = 0;						// Capture when last moved to check for inactivity shutdown

	void Init();
	void JoystickNotification(int16_t x, int16_t y, int16_t z);
	void Disconnect();
	bool EventHandler(hci_event_pckt* Event);
	void Calibrate();
	uint32_t SerialiseConfig(uint8_t** buff);
	uint32_t StoreConfig(uint8_t* buff);

private:
	enum HIDInfoFlags {RemoteWake = 1, NormallyConnectable = 2};
	enum Characteristic {ReportMap, HidInformation, ReportJoystick};
	enum CharOffset {ValueOffset = 1, DescriptorOffset = 2};

	static constexpr uint16_t JoystickReportID {0x0101};		// First Byte: Input Report (0x01) | Second Byte: Report ID (0x1 ... 0x03)

	const struct HIDInformation {
		uint16_t bcdHID {0x0101};					// Binary coded decimal HID version
		uint8_t  bcountryCode {0};  				// 0 = not localized
		uint8_t  flags = {RemoteWake | NormallyConnectable};	// See http://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.hid_information.xml
	} hidInformation;

	struct Pos3D {int16_t x; int16_t y; int16_t z;} joystickReport;
	struct {float x; float y; float z;} averageMovement;

	// Variables to track whether gyro is static and variations in measurements just noise
	Pos3D currPos, oldPos;
	static constexpr uint32_t compareLimit = 70;	// Difference of current x/y/z from previous position that counts as possible movement
	static constexpr uint32_t maxChange = 15;		// if there are more than maxChange measurement variations above compareLimit update moving bit array
	uint32_t countChange[8] = {};					// Store previous sums of 256 measurements potential movement sums
	uint8_t changeArrCounter = 0;
	uint8_t changeBitCounter = 0;
	uint8_t noMovementCnt = 0;

	uint16_t serviceHandle;				 			// Service handle
	uint16_t reportJoystickHandle;
	uint16_t reportJoystickRefDescHandle;
	uint16_t hidInformationHandle;
	uint16_t hidControlPointHandle;
	uint16_t reportMapHandle;
	uint16_t gyroCommandHandle;						// For receiving register read/write commands
	uint16_t gyroRegisterValHandle;					// For the gyroscope register value

	uint32_t lastSend = 0;							// For periodic sending of data, even if no movement
	uint32_t lastPrint = 0;							// For periodic printing out current gyroscope values

	static constexpr int32_t calibrateCount = 200;	// Total number of readings to use when calibrating
	int32_t calibrateCounter = 0;					// When calibrating offsets keeps count of readings averaged

	struct {
		uint8_t reg = 0;
		uint8_t val = 0;
	} gyroRegister;									// To allow gyroscope registers to be written and read over BLE

	void AppInit();
	static void UpdateJoystickReportChar();
	void UpdateReportMapChar();
	void UpdateHidInformationChar();
	void ControlPointWrite(uint16_t data);
	static void UpdateGyroChar();
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
