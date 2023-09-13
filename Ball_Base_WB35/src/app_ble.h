#pragma once
#include "app_conf.h"
#include "hci_tl.h"
#include <string>
#include "uartHandler.h"

// Advertising report created on the fly to preserve data for parsing and diagnostic output
struct AdvertisingReport {
	uint8_t address[6];
	uint16_t appearance;
	uint16_t serviceClasses;
	uint8_t flags;
	uint8_t manufactData[16];
	uint8_t manufactLen;
	std::string shortName;

	std::string formatAddress() {
		return HexByte(address[5]) + HexByte(address[4]) + HexByte(address[3]) + HexByte(address[2]) + HexByte(address[1]) + HexByte(address[0]);
	}
};


struct BleApp {
public:
	enum class ConnectionStatus {Idle, ClientConnected, Connecting};

	ConnectionStatus deviceConnectionStatus;
	static constexpr uint8_t bdddrSize = 6;
	bool coprocessorFailure = false;

	void Init();
	ConnectionStatus GetClientConnectionStatus(uint16_t connHandle);
	void ScanAndConnect();
	void GetHidReportMap(uint8_t* address);
	void ScanInfo();
	static void DisconnectRequest();
	void ServiceControlCallback(void* pckt);

private:
	enum class RequestAction {ScanConnect, ScanInfo, GetReportMap};
//	enum AddressTypes {PublicAddress = 0, RandomAddress = 1, ResolvablePrivateAddress = 2, NonResolvablePrivateAddress = 3};
	enum class IOCapability : uint8_t {DisplayOnly = 0, DisplayYesNo = 1, KeyboardOnly = 2, NoIO = 3, KeyboardDisplay = 4};
	enum class GapAddress : uint8_t {Public = 0, StaticRandom = 1, ResolvablePrivate = 2, NonResolvablePrivate = 3} ;
	enum class SecureSupport : uint8_t {NotSupported = 0, Optional = 1, Mandatory = 2};

	//static constexpr std::string_view GapDeviceName = "Ball_Remote";
	static constexpr uint8_t  TransmitPower = 24;					 	// -0.15dBm PA_Level Power amplifier output level (0-35)
	static constexpr uint8_t  DataLengthExtension = 1;					// Allows sending of app data payloads of up to 251 bytes (otherwise limited to 27 bytes)
	static constexpr uint16_t SlaveSleepClockAccuracy = 500;			// Sleep clock accuracy in Slave mode (ppm value)
	static constexpr uint16_t MasterSleepClockAccuracy = 0;				// Sleep clock accuracy in Master mode: 0 = 251 ppm to 500 ppm
	//static constexpr uint8_t  LowSpeedWakeUpClk = 1;					// Low speed clock for RF wake-up: 1 = HSE/32/32; 0 = external low speed crystal (no calibration)
	static constexpr uint32_t MaxConnEventLength = 0xFFFFFFFF;			// maximum duration of a slave connection event in units of 625/256 us (~2.44 us)
	static constexpr uint16_t HseStartupTime = 0x148;					// Startup time of high speed crystal oscillator in units of 625/256 us (~2.44 us)
	static constexpr uint8_t  ViterbiEnable = true;						// Enable Viterbi implementation in BLE LL reception
	static constexpr uint8_t  MaxCOChannels = 32;						// Maximum number of connection-oriented channels in initiator mode (0 - 64)
	//static constexpr uint8_t  MinTransmitPower = 20;					// Minimum transmit power in dBm supported by the Controller. Range: -127 - 20
	//static constexpr uint8_t  MaxTransmitPower = 20;					// Maximum transmit power in dBm supported by the Controller. Range: -127 - 20
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

	uint16_t connectionHandle;			// handle of the current active connection; When disconnected handle = 0xFFFF
	RequestAction action;
	std::string advMsg;
	uint8_t bd_addr_udn[bdddrSize];
	uint8_t deviceAddress[bdddrSize];

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

	static void ScanRequest();
	static void ConnectRequest();
	static void UserEvtRx(void* pPayload);
	static void StatusNot(HCI_TL_CmdStatus_t status);
	void PrintAdvData(std::unique_ptr<AdvertisingReport> ar);
	void HciGapGattInit();
	void TransportLayerInit();
	uint8_t* GetBdAddress();
};


extern BleApp bleApp;
