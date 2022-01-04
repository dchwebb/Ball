#pragma once

#include "stdint.h"
//#include <cstdlib>

//extern "C" {
enum class HidState {Idle, ClientConnected, DiscoverServices, DiscoverCharacs, DiscoverNotificationCharDesc, DiscoveringCharacs, EnableNotificationDesc, EnableHIDNotificationDesc};

typedef enum {
	PEER_CONN_HANDLE_EVT,
	PEER_DISCON_HANDLE_EVT,
} P2PC_APP_Opcode_Notification_evt_t;

typedef struct {
	P2PC_APP_Opcode_Notification_evt_t          P2P_Evt_Opcode;
	uint16_t                                    ConnectionHandle;
} P2PC_APP_ConnHandle_Not_evt_t;


void HID_APP_Init();
void HIDConnectionNotification(P2PC_APP_ConnHandle_Not_evt_t *pNotification);
HidState HID_Client_APP_Get_State();
void HID_APP_SW1_Button_Action();

//}
