#pragma once
#include "hci_tl.h"
#include <string>
#include "uartHandler.h"

typedef enum {
	APP_BLE_IDLE,
	APP_BLE_FAST_ADV,
	APP_BLE_LP_ADV,
	APP_BLE_SCAN,
	APP_BLE_LP_CONNECTING,
	APP_BLE_CONNECTED_SERVER,
	APP_BLE_CONNECTED_CLIENT,

	APP_BLE_DISCOVER_SERVICES,
	APP_BLE_DISCOVER_CHARACS,
	APP_BLE_DISCOVERING_CHARACS,
	APP_BLE_DISCOVER_WRITE_DESC,
	APP_BLE_DISCOVER_NOTIFICATION_CHAR_DESC,
	APP_BLE_ENABLE_NOTIFICATION_DESC,
	APP_BLE_ENABLE_HID_NOTIFICATION_DESC,
	APP_BLE_DISABLE_NOTIFICATION_DESC
} APP_BLE_ConnStatus_t;

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

	void clear() {
		appearance = 0;
		serviceClasses = 0;
		flags = 0;
		manufactLen = 0;
		shortName.clear();
	}
};

void APP_BLE_Init(void);
APP_BLE_ConnStatus_t APP_BLE_Get_Client_Connection_Status(uint16_t Connection_Handle);

void APP_BLE_Scan_and_Connect();
void APP_BLE_Key_Button2_Action();
void APP_BLE_Key_Button3_Action();
void APP_BLE_ScanInfo();
void PrintAdvData();
