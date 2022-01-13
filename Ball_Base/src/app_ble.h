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

	void Init();
	ConnectionStatus GetClientConnectionStatus(uint16_t connHandle);
	void ScanAndConnect();
	void GetHidReportMap(uint8_t* address);
	void ScanInfo();
	static void DisconnectRequest();
	void ServiceControlCallback(void* pckt);

private:
	enum class RequestAction {ScanConnect, ScanInfo, GetReportMap};
	enum AddressTypes {PublicAddress = 0, RandomAddress = 1, ResolvablePrivateAddress = 2, NonResolvablePrivateAddress = 3};

	bool deviceServerFound = false;
	AddressTypes deviceAddressType;

	uint16_t connectionHandle;			// handle of the current active connection; When disconnected handle = 0xFFFF
	RequestAction action;
	std::string advMsg;
	uint8_t bd_addr_udn[bdddrSize];
	uint8_t deviceAddress[bdddrSize];
	const uint8_t IdentityRootKey[16] = CFG_BLE_IRK;			// Identity root key used to derive LTK and CSRK
	const uint8_t EncryptionRootKey[16] = CFG_BLE_ERK;			// Encryption root key used to derive LTK and CSRK

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
