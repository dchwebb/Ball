#pragma once
#include "hci_tl.h"
#include <string>
#include "uartHandler.h"

#define BD_ADDR_SIZE_LOCAL    6

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


struct BleApplication {
public:
	enum class ConnectionStatus {Idle, ClientConnected, Connecting};


	void Init();
	ConnectionStatus GetClientConnectionStatus(uint16_t connHandle);
	void ScanAndConnect();
	void ScanInfo();
	static void DisconnectRequest();
	void ServiceControlCallback(void* pckt);

private:
	enum class RequestAction {ScanConnect, ScanInfo};

	bool deviceServerFound = false;
	ConnectionStatus deviceConnectionStatus;
	uint16_t connectionHandle;			// handle of the current active connection; When disconnected handle = 0xFFFF
	RequestAction action;
	std::string advMsg;
	uint8_t bd_addr_udn[BD_ADDR_SIZE_LOCAL];
	uint8_t remoteConnectAddress[BD_ADDR_SIZE_LOCAL];

	static void ScanRequest();
	static void ConnectRequest();
	static void UserEvtRx(void* pPayload);
	static void StatusNot(HCI_TL_CmdStatus_t status);
	void PrintAdvData(std::unique_ptr<AdvertisingReport> ar);
	void HciGapGattInit();
	void TransportLayerInit();
	uint8_t* GetBdAddress();
};



extern BleApplication bleApp;
