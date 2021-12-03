#pragma once

#include "hci_tl.h"

typedef enum
{
	APP_BLE_IDLE,
	APP_BLE_FAST_ADV,
	APP_BLE_LP_ADV,
	APP_BLE_SCAN,
	APP_BLE_LP_CONNECTING,
	APP_BLE_CONNECTED_SERVER,
	APP_BLE_CONNECTED_CLIENT
} APP_BLE_ConnStatus_t;

void APP_BLE_Init();
APP_BLE_ConnStatus_t APP_BLE_Get_Server_Connection_Status();
