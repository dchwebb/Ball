#pragma once
#include "app_conf.h"
#include "hci_tl.h"
#include <string>
#include <memory>

// Advertising report created on the fly to preserve data for parsing and diagnostic output
struct AdvertisingReport {
	uint8_t address[6];
	uint16_t appearance;
	uint16_t serviceClasses;
	uint8_t flags;
	uint8_t manufactData[16];
	uint8_t manufactLen;
	std::string shortName;
};


struct BleApp {
public:
	enum class ConnectionStatus {Idle, ClientConnected, Connecting} connectionStatus {ConnectionStatus::Idle};
	enum class RequestAction {None, ScanConnect, ScanInfo, GetReportMap} action {RequestAction::None};

	uint16_t connectionHandle;			// handle of the current active connection; When disconnected handle = 0xFFFF
	static constexpr uint8_t bleAddrSize = 6;
	bool coprocessorFailure = false;

	void Init();
	ConnectionStatus GetClientConnectionStatus(uint16_t connHandle);
	void SwitchConnectState();
	void GetHidReportMap(uint8_t* address);
	void ScanInfo();
	static void DisconnectRequest();
	void ServiceControlCallback(void* pckt);

private:

	enum class IOCapability : uint8_t {DisplayOnly = 0, DisplayYesNo = 1, KeyboardOnly = 2, NoIO = 3, KeyboardDisplay = 4};
	enum class GapAddress : uint8_t {Public = 0, StaticRandom = 1, ResolvablePrivate = 2, NonResolvablePrivate = 3} ;
	enum class SecureSupport : uint8_t {NotSupported = 0, Optional = 1, Mandatory = 2};

	static constexpr uint8_t  TransmitPower = 24;					 	// -0.15dBm PA_Level Power amplifier output level (0-35)
	static constexpr uint8_t  DataLengthExtension = 1;					// Allows sending of app data payloads of up to 251 bytes (otherwise limited to 27 bytes)
	static constexpr uint16_t SlaveSleepClockAccuracy = 500;			// Sleep clock accuracy in Slave mode (ppm value)
	static constexpr uint16_t MasterSleepClockAccuracy = 0;				// Sleep clock accuracy in Master mode: 0 = 251 ppm to 500 ppm
	static constexpr uint8_t  LowSpeedWakeUpClk = 1;					// Low speed clock for RF wake-up: 1 = HSE/32/32; 0 = external low speed crystal (no calibration)
	static constexpr uint32_t MaxConnEventLength = 0xFFFFFFFF;			// maximum duration of a slave connection event in units of 625/256 us (~2.44 us)
	static constexpr uint16_t HseStartupTime = 0x148;					// Startup time of high speed crystal oscillator in units of 625/256 us (~2.44 us)
	static constexpr uint8_t  ViterbiEnable = true;						// Enable Viterbi implementation in BLE LL reception
	static constexpr uint8_t  MaxCOChannels = 32;						// Maximum number of connection-oriented channels in initiator mode (0 - 64)
	static constexpr uint8_t  MinTransmitPower = 0;						// Minimum transmit power in dBm supported by the Controller. Range: -127 - 20
	static constexpr uint8_t  MaxTransmitPower = 0;						// Maximum transmit power in dBm supported by the Controller. Range: -127 - 20
	static constexpr uint8_t  RXModelConfig = 0;						// 0: Legacy agc_rssi model
	static constexpr uint8_t  MaxAdvertisingSets = 3;					// Maximum number of advertising sets. Range: 1 = 8 with limitations
 	static constexpr uint16_t MaxAdvertisingDataLength = 1650;			// Maximum advertising data length (in bytes) Range: 31 - 1650 with limitation
	static constexpr int16_t  TXPathCompensation = 0;					// RF TX Path Compensation Value Units: 0.1 dB Range: -1280 .. 1280
	static constexpr int16_t  RXPathCompensation = 0;					// RF RX Path Compensation Value Units: 0.1 dB Range: -1280 .. 1280
	static constexpr uint8_t  BLECoreVersion = 12;

	// Note that the GAP and GATT services are automatically added so this parameter should be 2 plus the number of user services
	static constexpr uint16_t NumberOfGATTServices = 8;					// Max number of Services stored in the GATT database.

	// number of characteristic + number of characteristic values + number of descriptors, excluding the services
	// Note: some characteristics and relative descriptors added automatically at initialization so this parameter is 9 + number of user Attributes
	static constexpr uint16_t NumberOfGATTAttributes = 68;

	// Keys used to derive LTK and CSRK
	static constexpr uint8_t IdentityRootKey[16] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
	static constexpr uint8_t EncryptionRootKey[16] = {0xFE, 0xDC, 0xBA, 0x09, 0x87, 0x65, 0x43, 0x21, 0xFE, 0xDC, 0xBA, 0x09, 0x87, 0x65, 0x43, 0x21};

	bool deviceServerFound = false;
	GapAddress deviceAddressType;
	uint8_t bd_addr_udn[bleAddrSize];
	uint8_t deviceAddress[bleAddrSize];

	struct {
		uint8_t ioCapability = (uint8_t)IOCapability::NoIO;				// IO capability of the device
		uint8_t mitmMode = true;										// Man In the Middle protection required?
		uint8_t bondingMode = true;										// bonding mode of the device
		uint8_t useFixedPin = false;									// 0 = use fixed pin; 1 = request for passkey
		uint8_t encryptionKeySizeMin = 8;								// minimum encryption key size requirement
		uint8_t encryptionKeySizeMax = 16;								// maximum encryption key size requirement
		uint8_t secureSupport = (uint8_t)SecureSupport::Optional; 		// LE Secure connections support
		uint8_t keypressNotificationSupport = false;					// No support for keyboard
		uint8_t BLEAddressType = (uint8_t)GapAddress::Public;			// BLE Address Type
		uint32_t fixedPin = 111111;										// Fixed pin for pairing process if Use_Fixed_Pin = 1
	} Security;

	// Connection settings
	static constexpr uint16_t leScanInterval = 500.0f / 0.625f;		// ms: Interval between LE scans
	static constexpr uint16_t leScanWindow = 500.0f / 0.625f;		// ms: Duration of LE scan
	static constexpr uint16_t connIntervalMin = 7.5f / 1.25f;		// ms: Minimum connection event interval
	static constexpr uint16_t connIntervalMax = 20.0f / 1.25f;		// ms: Maximum connection event interval
	static constexpr uint16_t connLatency = 0;						// Max peripheral latency in number of connection events
	static constexpr uint16_t supervisionTimeout = 500;				// N * 10 ms: Supervision timeout for the LE Link
	static constexpr uint16_t minimumCELength = 10.0f / 0.625f;		// 10ms Minimum time needed for the LE connection
	static constexpr uint16_t maximumCELength = 10.0f / 0.625f;		// 10ms Maximum time needed for the LE connection


	static void ScanRequest();
	static void ConnectRequest();
	static void UserEvtRx(void* pPayload);
	static void StatusNot(HCI_TL_CmdStatus_t status);
	void PrintAdvData(std::unique_ptr<AdvertisingReport> ar);
	void HciGapGattInit();
	uint8_t* GetBdAddress();
};


extern BleApp bleApp;
