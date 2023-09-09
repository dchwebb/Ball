#pragma once

#include "hci_tl.h"
#include "ble_defs.h"
#include "app_conf.h"
#include <string>

struct BleApp {
public:
	enum class ConnStatus {Idle, FastAdv, LPAdv, Scan, LPConnecting, Connected};
	enum class LowPowerMode {Stop, Shutdown};
	static constexpr uint8_t bdAddrSize = 6;

	uint16_t connectionHandle = 0xFFFF;									// When disconnected handle is set to 0xFFFF
	ConnStatus connectionStatus = ConnStatus::Idle;
	LowPowerMode lowPowerMode {LowPowerMode::Stop};
	bool coprocessorFailure = false;

	void Init();
	void ServiceControlCallback(hci_event_pckt* pckt);
	static void DisconnectRequest();
private:
	enum class SleepState {CancelAdv, GoToSleep, Awake};
	enum class IOCapability : uint8_t {DisplayOnly = 0, DisplayYesNo = 1, KeyboardOnly = 2, NoIO = 3, KeyboardDisplay = 4};
	enum class AdvertisingType : uint8_t {Indirect = 0, DirectIndirect = 1, ScanIndirect = 2, NonConnInd = 3, DirectIndirectLDC = 4};
	enum class GapAddress : uint8_t {Public = 0, StaticRandom = 1, ResolvablePrivate = 2, NonResolvablePrivate = 3} ;
	enum class SecureSupport : uint8_t {NotSupported = 0, Optional = 1, Mandatory = 2};

	// Advertising Data: Length | Type | Data
	const uint8_t ad_data[25] = {
		2, AD_TYPE_TX_POWER_LEVEL, 0, 									// Transmission Power -0.15dBm
		9, AD_TYPE_COMPLETE_LOCAL_NAME, 'M', 'O', 'U', 'N', 'T', 'J', 'O', 'Y', 	// Complete name
		3, AD_TYPE_16_BIT_SERV_UUID_CMPLT_LIST, 0x12, 0x18,				// 0x1812 = HID
		7, AD_TYPE_MANUFACTURER_SPECIFIC_DATA, 0x01, 0x83, 0xDE, 0xAD, 0xBE, 0xEF
	};

	static constexpr uint8_t TransmitPower = 16;				 		// PA_Level Power amplifier output level (0-35)

	static constexpr uint8_t IdentityRootKey[16] = CFG_BLE_IRK;			// Identity root key used to derive LTK and CSRK
	static constexpr uint8_t EncryptionRootKey[16] = CFG_BLE_ERK;		// Encryption root key used to derive LTK and CSRK
	static constexpr uint32_t FastAdvTimeout = (30 * 1000 * 1000 / CFG_TS_TICK_VAL); 	// 30s
	static constexpr uint16_t FastAdvIntervalMin = 128;					// Intervals for fast advertising
	static constexpr uint16_t FastAdvIntervalMax = 160;
	static constexpr uint16_t LPAdvIntervalMin = 1600;					// Intervals for low power advertising
	static constexpr uint16_t LPAdvIntervalMax = 4000;
	static constexpr std::string_view GapDeviceName = "Ball_Remote";

	uint8_t bd_addr_udn[bdAddrSize];
	uint8_t lowPowerAdvTimerId;
	SleepState sleepState {SleepState::Awake};

	struct  {
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

	void EnableAdvertising(ConnStatus status);
	static void SwitchLPAdvertising();
	static void SwitchFastAdvertising();
	static void QueueLPAdvertising();
	static void QueueFastAdvertising();
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
