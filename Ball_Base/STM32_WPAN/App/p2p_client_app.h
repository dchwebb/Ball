#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	PEER_CONN_HANDLE_EVT,
	PEER_DISCON_HANDLE_EVT,
} P2PC_APP_Opcode_Notification_evt_t;

typedef struct {
	P2PC_APP_Opcode_Notification_evt_t          P2P_Evt_Opcode;
	uint16_t                                    ConnectionHandle;
} P2PC_APP_ConnHandle_Not_evt_t;


void P2PC_APP_Init(void);
void P2PC_APP_Notification(P2PC_APP_ConnHandle_Not_evt_t *pNotification);
uint8_t P2P_Client_APP_Get_State(void);
void P2PC_APP_SW1_Button_Action(void);

#ifdef __cplusplus
}
#endif

