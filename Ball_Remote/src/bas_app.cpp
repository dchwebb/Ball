#include "common_blesvc.h"
#include "stm32_seq.h"
#include "bas_app.h"

//static SVCCTL_EvtAckStatus_t BAS_Event_Handler(void *Event);

typedef struct {
	uint16_t SvcHdle;					// Service handle
	uint16_t BatteryLevelHandle;		// Characteristic handle
} BAS_Context_t;

PLACE_IN_SECTION("BLE_DRIVER_CONTEXT") static BAS_Context_t BAS_Context;

typedef struct {
	uint16_t  Level;
	bool      BAS_Notification_Status;
} BAS_APP_Context_t;

PLACE_IN_SECTION("BLE_APP_CONTEXT") BAS_APP_Context_t BAS_App_Context;


void BAS_Init()
{
	uint16_t uuid;
	tBleStatus hciCmdResult = BLE_STATUS_SUCCESS;

	//SVCCTL_RegisterSvcHandler(BAS_Event_Handler);	// Register the event handler to the BLE controller

	uuid = BATTERY_SERVICE_UUID;
	hciCmdResult = aci_gatt_add_service(UUID_TYPE_16,
			(Service_UUID_t*)&uuid,
			PRIMARY_SERVICE,
			4,										// Max_Attribute_Records
			&(BAS_Context.SvcHdle));
	APP_DBG_MSG("- BAS: Registered BAS Service handle: 0x%X\n", BAS_Context.SvcHdle);

	uuid = BATTERY_LEVEL_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(BAS_Context.SvcHdle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			4,										// Char value length
			CHAR_PROP_READ | CHAR_PROP_NOTIFY,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 									// encryKeySize
			CHAR_VALUE_LEN_CONSTANT,
			&(BAS_Context.BatteryLevelHandle));
	APP_DBG_MSG("- BAS: Registered Battery Level characteristic handle: 0x%X\n", BAS_Context.BatteryLevelHandle);

	if (hciCmdResult != BLE_STATUS_SUCCESS) {
		APP_DBG_MSG("- BAS: Error registering characteristics: 0x%X\n", hciCmdResult);
	}
}


void BAS_App_Init()
{
	UTIL_SEQ_RegTask(1 << CFG_TASK_BAS_LEVEL, UTIL_SEQ_RFU, BAS_App_Send_Notification);		// FIXME - not currently using
	BAS_App_Context.BAS_Notification_Status = false;			// Initialise notification info
	BAS_App_Context.Level = 0x42;								// Initialize Level
	BAS_Update_Char();
}


// Update Battery level characteristic value
tBleStatus BAS_Update_Char()
{
	return aci_gatt_update_char_value(BAS_Context.SvcHdle, BAS_Context.BatteryLevelHandle, 0, 2, (uint8_t*)&BAS_App_Context.Level);
}


void BAS_App_Send_Notification()
{
	BAS_Update_Char();
}


void BAS_App_Set_Level(uint8_t level)
{
	BAS_App_Context.Level = level;
	if (BAS_App_Context.BAS_Notification_Status) {
		BAS_Update_Char();
	} else {
		printf("- BAS: Notifications disabled (set to %d)\r\n", level);
	}
}


SVCCTL_EvtAckStatus_t BAS_Event_Handler(void *Event)
{
	SVCCTL_EvtAckStatus_t return_value = SVCCTL_EvtNotAck;
	hci_event_pckt* event_pckt = (hci_event_pckt *)(((hci_uart_pckt*)Event)->data);

	if (event_pckt->evt == HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE) {
		evt_blecore_aci* blecore_evt = (evt_blecore_aci*)event_pckt->data;

		switch (blecore_evt->ecode) {
			case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
			{
				aci_gatt_attribute_modified_event_rp0* attribute_modified = (aci_gatt_attribute_modified_event_rp0*)blecore_evt->data;
				if (attribute_modified->Attr_Handle == (BAS_Context.BatteryLevelHandle + 2)) {		// 2 = Offset of descriptor from characteristic handle
					return_value = SVCCTL_EvtAckFlowEnable;

					if (attribute_modified->Attr_Data[0] == 1) {
						BAS_App_Context.BAS_Notification_Status = true;
						BAS_App_Send_Notification();
					} else {
						BAS_App_Context.BAS_Notification_Status = false;
					}
				}
				break;
			}

			case ACI_GATT_READ_PERMIT_REQ_VSEVT_CODE:
			{
				aci_gatt_read_permit_req_event_rp0* read_req = (aci_gatt_read_permit_req_event_rp0*)blecore_evt->data;
				if (read_req->Attribute_Handle == (BAS_Context.BatteryLevelHandle + 1)) {		// 1 = Offset of value from characteristic handle
					return_value = SVCCTL_EvtAckFlowEnable;
					aci_gatt_allow_read(read_req->Connection_Handle);
				}
				break;
			}

			default:
				break;
		}

	}

	return return_value;
}

