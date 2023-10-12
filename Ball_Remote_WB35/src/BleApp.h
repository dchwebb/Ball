#pragma once

#include "hci_tl.h"
#include "ble_defs.h"
#include "app_conf.h"
#include <string>

struct BleApp {
public:
	enum class ConnStatus {Idle, FastAdv, LPAdv, Scan, Connected} connectionStatus {ConnStatus::Idle};
	enum class SleepState {Awake, RequestSleep, CancelAdv, GoToSleep, WakeToShutdown} sleepState {SleepState::Awake};
	enum class LowPowerMode {Sleep, Shutdown} lowPowerMode;		// Specify low power mode when sleep requested
	enum class WakeAction {LPAdvertising, Shutdown} wakeAction;			// Action to carry out when waking up from RTC interrupt
	static constexpr uint8_t bdAddrSize = 6;

	uint16_t connectionHandle = 0xFFFF;									// When disconnected handle is set to 0xFFFF
	bool coprocessorFailure = false;
	bool motionWakeup = false;											// Set when sleeping and woken up with motion detected

	// Settings that can be saved to flash - timeouts in seconds
	struct Settings {
		uint32_t fastAdvTimeout = 30;								 	// Period before fast advertising switched to low power (secs)
		uint32_t lpAdvTimeout = 60;									 	// Shutdown timeout if not connected while LP advertising (secs)
		uint32_t inactiveTimeout = 600;									// Shutdown timeout if no activity whilst connected (secs)
		uint8_t  transmitPower = 16;				 					// PA_Level Power amplifier output level (0-35)
		bool     sleepUsesHSI = false;									// Switch to HSI when sleeping which uses lower power but can cause issues waking up
	} settings;

	void Init();
	static void DisconnectRequest();
	void InactivityTimeout();											// Called from HidService to indicate no movement timeout
	void RTCWakeUp();													// Handle TRC interrupt setting appropriate next actions
	uint32_t SerialiseConfig(uint8_t** buff);
	uint32_t StoreConfig(uint8_t* buff);

private:
	enum class IOCapability : uint8_t {DisplayOnly = 0, DisplayYesNo = 1, KeyboardOnly = 2, NoIO = 3, KeyboardDisplay = 4};
	enum class AdvertisingType : uint8_t {Indirect = 0, DirectIndirect = 1, ScanIndirect = 2, NonConnInd = 3, DirectIndirectLDC = 4};
	enum class GapAddress : uint8_t {Public = 0, StaticRandom = 1, ResolvablePrivate = 2, NonResolvablePrivate = 3} ;
	enum class SecureSupport : uint8_t {NotSupported = 0, Optional = 1, Mandatory = 2};
	enum PreferredPhy : uint8_t {PreferAllPhy = 0, Prefer1MbitPhy = 1, Prefer2MbitPhy = 2};


	// Advertising Data: Length | Type | Data
	const uint8_t ad_data[25] = {
		2, AD_TYPE_TX_POWER_LEVEL, 0, 									// Transmission Power -0.15dBm
		9, AD_TYPE_COMPLETE_LOCAL_NAME, 'M', 'O', 'U', 'N', 'T', 'J', 'O', 'Y', 	// Complete name
		3, AD_TYPE_16_BIT_SERV_UUID_CMPLT_LIST, 0x12, 0x18,				// 0x1812 = HID
		7, AD_TYPE_MANUFACTURER_SPECIFIC_DATA, 0x01, 0x83, 0xDE, 0xAD, 0xBE, 0xEF
	};

	static constexpr std::string_view GapDeviceName = "Ball_Remote";
	static constexpr uint16_t FastAdvIntervalMin = 128;					// Intervals for fast advertising
	static constexpr uint16_t FastAdvIntervalMax = 160;
	static constexpr uint16_t LPAdvIntervalMin = 1600;					// Intervals for low power advertising
	static constexpr uint16_t LPAdvIntervalMax = 4000;
	static constexpr uint8_t  DataLengthExtension = 1;					// Allows sending of app data payloads of up to 251 bytes (otherwise limited to 27 bytes)
	static constexpr uint16_t SlaveSleepClockAccuracy = 500;			// Sleep clock accuracy in Slave mode (ppm value)
	static constexpr uint16_t MasterSleepClockAccuracy = 0;				// Sleep clock accuracy in Master mode: 0 = 251 ppm to 500 ppm
	static constexpr uint8_t  LowSpeedWakeUpClk = 1;					// Low speed clock for RF wake-up: 1 = HSE/32/32; 0 = external low speed crystal (no calibration)
	static constexpr uint32_t MaxConnEventLength = 0xFFFFFFFF;			// maximum duration of a slave connection event in units of 625/256 us (~2.44 us)
	static constexpr uint16_t HseStartupTime = 0x148;					// Startup time of high speed crystal oscillator in units of 625/256 us (~2.44 us)
	static constexpr uint8_t  ViterbiEnable = true;						// Enable Viterbi implementation in BLE LL reception
	static constexpr uint8_t  MaxCOChannels = 32;						// Maximum number of connection-oriented channels in initiator mode (0 - 64)
	static constexpr uint8_t  MinTransmitPower = 20;					// Minimum transmit power in dBm supported by the Controller. Range: -127 - 20
	static constexpr uint8_t  MaxTransmitPower = 20;					// Maximum transmit power in dBm supported by the Controller. Range: -127 - 20
	static constexpr uint8_t  RXModelConfig = 0;						// 0: Legacy agc_rssi model
	static constexpr uint8_t  MaxAdvertisingSets = 3;					// Maximum number of advertising sets. Range: 1 = 8 with limitations
 	static constexpr uint16_t MaxAdvertisingDataLength = 1650;			// Maximum advertising data length (in bytes) Range: 31 - 1650 with limitation
	static constexpr int16_t  TXPathCompensation = 0;					// RF TX Path Compensation Value Units: 0.1 dB Range: -1280 .. 1280
	static constexpr int16_t  RXPathCompensation = 0;					// RF RX Path Compensation Value Units: 0.1 dB Range: -1280 .. 1280
	static constexpr uint8_t  BLECoreVersion = 12; 						// BLE core specification version: 11(5.2), 12(5.3)


	// Note that the GAP and GATT services are automatically added so this parameter should be 2 plus the number of user services
	static constexpr uint16_t NumberOfGATTServices = 8;					// Max number of Services stored in the GATT database.

	// number of characteristic + number of characteristic values + number of descriptors, excluding the services
	// Note: some characteristics and relative descriptors added automatically at initialization so this is 9 + number of user Attributes
	static constexpr uint16_t NumberOfGATTAttributes = 128;				// Max number of attributes stored in GATT database

	// Keys used to derive LTK and CSRK
	static constexpr uint8_t IdentityRootKey[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
	static constexpr uint8_t EncryptionRootKey[16] = {0xFE, 0xDC, 0xBA, 0x09, 0x87, 0x65, 0x43, 0x21, 0xFE, 0xDC, 0xBA, 0x09, 0x87, 0x65, 0x43, 0x21};

	uint8_t bdAddrUdn[bdAddrSize];

	struct {
		uint8_t ioCapability = (uint8_t)IOCapability::NoIO;				// IO capability of the device
		uint8_t mitmMode = false;										// Man In the Middle protection required?
		uint8_t bondingMode = true;										// bonding mode of the device
		uint8_t useFixedPin = false;									// 0 = use fixed pin; 1 = request for passkey
		uint8_t encryptionKeySizeMin = 8;								// minimum encryption key size requirement
		uint8_t encryptionKeySizeMax = 16;								// maximum encryption key size requirement
		uint8_t secureSupport = (uint8_t)SecureSupport::Optional; 		// LE Secure connections support
		uint8_t keypressNotificationSupport = false;					// No support for keyboard
		uint8_t BLEAddressType = (uint8_t)GapAddress::Public;			// BLE Address Type
		uint32_t fixedPin = 111111;										// Fixed pin for pairing process if Use_Fixed_Pin = 1
	} Security;

	void ServiceControlCallback(hci_event_pckt* pckt);
	void EnableAdvertising(ConnStatus status);
	static void SwitchLPAdvertising();
	static void CancelAdvertising();
	static void GoToSleep();
	static void GoToShutdown();
	static void EnterSleepMode();
	void WakeFromSleep();

	static void StatusNotify(HCI_TL_CmdStatus_t status);
	void HciGapGattInit();
	uint8_t* GetBdAddress();
	static void UserEvtRx(void* pPayload);
	void TransportLayerInit();
};


extern BleApp bleApp;
