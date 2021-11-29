#include "main.h"
#include "app_common.h"
#include "dbg_trace.h"
#include "ble.h"
#include "custom_app.h"
#include "custom_stm.h"
#include "stm32_seq.h"

typedef struct {
	uint8_t      Mjchar_n_Notification_Status;
	struct {
		uint16_t TimeStamp;
		uint16_t Value;
	} Temperature;
	uint16_t     ConnectionHandle;
} Custom_App_Context_t;

PLACE_IN_SECTION("BLE_APP_CONTEXT") static Custom_App_Context_t Custom_App_Context;

uint8_t UpdateCharData[247];
uint8_t NotifyCharData[247];
uint8_t SecureReadData;

static void Custom_Mjchar_n_Update_Char(void);
static void Custom_Mjchar_n_Send_Notification(void);

void Custom_STM_App_Notification(Custom_STM_App_Notification_evt_t *pNotification)
{
	switch(pNotification->Custom_Evt_Opcode) {
	case CUSTOM_STM_MJCHAR_W_READ_EVT:
		break;

	case CUSTOM_STM_MJCHAR_W_WRITE_NO_RESP_EVT:
		break;

	case CUSTOM_STM_MJCHAR_N_NOTIFY_ENABLED_EVT:
		Custom_App_Context.Mjchar_n_Notification_Status = 1;         /* notification status has been enabled */
		Custom_Mjchar_n_Send_Notification();
		break;

	case CUSTOM_STM_MJCHAR_N_NOTIFY_DISABLED_EVT:
		Custom_App_Context.Mjchar_n_Notification_Status = 0;         /* notification status has been disabled */
		break;

	default:
		break;
	}
	return;
}

void Custom_APP_Notification(Custom_App_ConnHandle_Not_evt_t *pNotification)
{
	switch(pNotification->Custom_Evt_Opcode) {
	case CUSTOM_CONN_HANDLE_EVT:
		break;

	case CUSTOM_DISCON_HANDLE_EVT:
		break;

	default:
		break;
	}
	return;
}


void Custom_APP_Init(void)
{
	Custom_App_Context.Temperature.TimeStamp = 0;
	Custom_App_Context.Temperature.Value = 37;
	UTIL_SEQ_RegTask(1 << CFG_TASK_NOTIFY_TEMPERATURE, 0, Custom_Mjchar_n_Send_Notification);
	return;
}


void Custom_Mjchar_n_Update_Char(void) /* Property Read */
{
	Custom_STM_App_Update_Char(CUSTOM_STM_MJCHAR_N, (uint8_t *)UpdateCharData);
	return;
}


void Custom_Mjchar_n_Send_Notification(void) /* Property Notification */
{
	if (Custom_App_Context.Mjchar_n_Notification_Status) {
		Custom_STM_App_Update_Char(CUSTOM_STM_MJCHAR_N, (uint8_t *)NotifyCharData);
		Custom_App_Context.Temperature.Value++;
		NotifyCharData[0] = (uint8_t)(Custom_App_Context.Temperature.TimeStamp & 0x00FF);
		NotifyCharData[1] = (uint8_t)(Custom_App_Context.Temperature.TimeStamp >> 8);
		NotifyCharData[2] = (uint8_t)(Custom_App_Context.Temperature.Value & 0x00FF);
		NotifyCharData[3] = (uint8_t)(Custom_App_Context.Temperature.Value >> 8);
	} else {
		APP_DBG_MSG("-- CUSTOM APPLICATION : CAN'T INFORM CLIENT -  NOTIFICATION DISABLED\n ");
	}
	return;
}
