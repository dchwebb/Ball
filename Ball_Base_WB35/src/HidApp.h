#pragma once

#include "stdint.h"
#include "svc_ctl.h"


struct HidApp {
public:
	// state of the HID Client state machine
	enum class HidState {Idle, ClientConnected, Disconnect, DiscoverServices, DiscoverCharacteristics, DiscoveredCharacteristics, DiscoveringCharacteristics,
		EnableNotificationDesc, EnableHIDNotificationDesc} state;

	enum class HidAction {None, Connect, GetReportMap, BatteryLevel, GyroRead} action;

	struct Pos3D {
		float x = 2047.0;
		float y = 2047.0;
		float z = 2047.0;
	} position3D;

	Pos3D offset = {0.0f, 0.0f, 0.0f};				// Calibrated adjustment for gyroscope offsets and drift
	float divider = 600.0f;							// Divider for raw gyroscope data (increase for more sensitivity)
	bool outputGyro = false;						// Set to true to output gyro data periodically
	bool outputBattery = false;						// Set to true to output battery level when received
	uint8_t batteryLevel = 0;						// Battery level as notified by remote unit

	// Used to read and write gyro registers
	uint16_t gyroCommand = 0;						// If reading a register set to a single byte value; if writing set register and value
	enum class GyroCmdType : uint8_t {read = 1, write = 2} gyroCmdType;

	void Init();
	void HIDConnectionNotification();
	void Calibrate();
	void GetReportMap();
	static void GetBatteryLevel();
	static void GyroCommand();
	static void ReadGyroRegister();
	uint32_t SerialiseConfig(uint8_t** buff);
	uint32_t StoreConfig(uint8_t* buff);

private:
	static constexpr uint16_t GYRO_CMD_CHAR_UUID = 0xFE40;
	static constexpr uint16_t GYRO_REG_CHAR_UUID = 0xFE41;

	uint16_t connHandle;							// connection handle

	uint16_t hidServiceHandle;						// handle of the HID service
	uint16_t hidServiceEndHandle;					// end handle of the HID service
	uint16_t hidNotificationCharHandle;				// handle of the HID Report characteristic
	uint16_t hidNotificationDescHandle;				// handle of the HID report client configuration descriptor
	uint16_t batteryServiceHandle;					// handle of the Battery service
	uint16_t batteryServiceEndHandle;				// end handle of the Battery service
	uint16_t batteryNotificationCharHandle;			// handle of the Rx characteristic - Notification From Server
	uint16_t batteryNotificationDescHandle;			// handle of the client configuration descriptor of Rx characteristic
	uint16_t hidReportMapHandle;					// handle of report map
	uint16_t gyroCmdNotificationCharHandle;			// Gyroscope command read/write handle
	uint16_t gyroCmdNotificationDescHandle;			// Gyroscope command read/write handle
	uint16_t gyroRegNotificationCharHandle;			// Gyroscope register read/write handle
	uint16_t gyroRegNotificationDescHandle;			// Gyroscope register read/write handle

	static constexpr float calibFilter = 0.9999f;	// 1 pole filter co-efficient for smoothing long-term drift adjustments
	static constexpr int32_t calibrateCount = 200;	// Total number of readings to use when calibrating
	int32_t calibrateCounter = 0;					// When calibrating offsets keeps count of readings averaged
	float calibX, calibY, calibZ;					// Temporary totals used during calibration
	uint32_t lastPrint = 0;							// For perdiodic printing of gyro output

	static SVCCTL_EvtAckStatus_t HIDEventHandler(void *Event);
	static void HIDServiceDiscovery();
	void HidNotification(int16_t* payload, uint8_t len);
	void BatteryNotification(uint8_t* payload, uint8_t len);
	void PrintReportMap(uint8_t* data, uint8_t len);
};

extern HidApp hidApp;
