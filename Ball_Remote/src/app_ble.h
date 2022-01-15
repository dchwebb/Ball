#pragma once

#include "hci_tl.h"
#include "ble_defs.h"

struct BleApp {
public:
	enum class ConnStatus {Idle, FastAdv, LPAdv, Scan, LPConnecting, Connected};
	static constexpr uint8_t bdAddrSize = 6;

	uint16_t connectionHandle = 0xFFFF;									// When disconnected handle is set to 0xFFFF
	ConnStatus connectionStatus = ConnStatus::Idle;

	void Init();
	void ServiceControlCallback(void* pckt);
private:

	// Advertising Data: Length | Type | Data
	const uint8_t ad_data[25] = {
		2, AD_TYPE_TX_POWER_LEVEL, 0, 									// Transmission Power -0.15dBm
		9, AD_TYPE_COMPLETE_LOCAL_NAME, 'M', 'O', 'U', 'N', 'T', 'J', 'O', 'Y', 	// Complete name
		3, AD_TYPE_16_BIT_SERV_UUID_CMPLT_LIST, 0x12, 0x18,				// 0x1812 = HID
		7, AD_TYPE_MANUFACTURER_SPECIFIC_DATA, 0x01, 0x83, 0xDE, 0xAD, 0xBE, 0xEF
	};

	static constexpr uint8_t IdentityRootKey[16] = CFG_BLE_IRK;			// Identity root key used to derive LTK and CSRK
	static constexpr uint8_t EncryptionRootKey[16] = CFG_BLE_ERK;		// Encryption root key used to derive LTK and CSRK
	static constexpr uint32_t FastAdvTimeout = (30 * 1000 * 1000 / CFG_TS_TICK_VAL); 	// 30s
	static constexpr uint16_t FastAdvIntervalMin = 128;					// Intervals for fast advertising
	static constexpr uint16_t FastAdvIntervalMax = 160;
	static constexpr uint16_t LPAdvIntervalMin = 1600;					// Intervals for low power advertising
	static constexpr uint16_t LPAdvIntervalMax = 4000;

	uint8_t bd_addr_udn[bdAddrSize];
	uint8_t lowPowerAdvTimerId;											// ID of timer used to switch to low power avertising

	struct  {
		uint8_t ioCapability = CFG_IO_CAPABILITY;						// IO capability of the device
		uint8_t mitmMode = CFG_MITM_PROTECTION;							// Man In the Middle protection required?
		uint8_t bondingMode = CFG_BONDING_MODE;							// bonding mode of the device
		uint8_t Use_Fixed_Pin = CFG_USED_FIXED_PIN;						// 0 = use fixed pin; 1 = request for passkey
		uint8_t encryptionKeySizeMin = CFG_ENCRYPTION_KEY_SIZE_MIN;		// minimum encryption key size requirement
		uint8_t encryptionKeySizeMax = CFG_ENCRYPTION_KEY_SIZE_MAX;		// maximum encryption key size requirement
		uint32_t fixedPin = CFG_FIXED_PIN;								// Fixed pin for pairing process if Use_Fixed_Pin = 1
	} Security;

	void EnableAdvertising(ConnStatus status);
	static void SwitchLPAdvertising();
	static void CancelAdvertising();
	static void QueueLPAdvertising();
	static void StatusNot(HCI_TL_CmdStatus_t status);
	void HciGapGattInit();
	uint8_t* GetBdAddress();
	static void UserEvtRx(void* pPayload);
	void TransportLayerInit();
};


extern BleApp bleApp;
