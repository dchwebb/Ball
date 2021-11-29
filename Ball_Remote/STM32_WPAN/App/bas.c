#include "common_blesvc.h"
#include "stm32_seq.h"

static SVCCTL_EvtAckStatus_t BAS_Event_Handler(void *Event);

typedef struct {
	uint16_t SvcHdle;				  /**< Service handle */
	uint16_t BatteryLevelHandle;   	  /**< Characteristic handle */
} BAS_Context_t;

PLACE_IN_SECTION("BLE_DRIVER_CONTEXT") static BAS_Context_t BAS_Context;

typedef struct {
	uint8_t   BAS_Notification_Status;
	uint16_t  Level;
	uint8_t   TimerLevel_Id;
} BAS_APP_Context_t;

PLACE_IN_SECTION("BLE_APP_CONTEXT") BAS_APP_Context_t BAS_App_Context;

void BAS_Init(void)
{
	uint16_t uuid;
	tBleStatus hciCmdResult = BLE_STATUS_SUCCESS;

	// Register the event handler to the BLE controller
	SVCCTL_RegisterSvcHandler(BAS_Event_Handler);

	uuid = BATTERY_SERVICE_UUID;
	hciCmdResult = aci_gatt_add_service(UUID_TYPE_16,
			(Service_UUID_t *) &uuid,
			PRIMARY_SERVICE,
			4,		// Max_Attribute_Records
			&(BAS_Context.SvcHdle));

	uuid = BATTERY_LEVEL_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(BAS_Context.SvcHdle,
			UUID_TYPE_16,
			(Char_UUID_t *) &uuid ,
			4,				// Char value length
			CHAR_PROP_READ | CHAR_PROP_NOTIFY,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP, /* gattEvtMask */
			10, /* encryKeySize */
			CHAR_VALUE_LEN_CONSTANT, /* isVariable */
			&(BAS_Context.BatteryLevelHandle));

	return;
}

void BAS_App_Init(void)
{
	UTIL_SEQ_RegTask(1 << CFG_TASK_BAS_LEVEL, UTIL_SEQ_RFU, BAS_App_Send_Notification);
	//UTIL_SEQ_RegTask(1 << CFG_TASK_NOTIFY_TEMPERATURE, 0, Custom_Mjchar_n_Send_Notification);

	// Initialise notification info
	BAS_App_Context.BAS_Notification_Status = 0;

	// Initialize Level
	BAS_App_Context.Level = 0x42;
	BAS_Update_Char(BATTERY_LEVEL_CHAR_UUID, (uint8_t *)&BAS_App_Context.Level);

	// Create timer for Battery Level
	//HW_TS_Create(CFG_TIM_PROC_ID_ISR, &(BAS_App_Context[index].TimerLevel_Id), hw_ts_Repeated, BAS_App_UpdateLevel);

	return;
}


/**
 * @brief  Characteristic update
 * @param  CharOpcode: Characteristic identifier
 * @param  Service_Instance: Instance of the service to which the characteristic belongs
 *
 */
tBleStatus BAS_Update_Char(uint16_t CharOpcode, uint8_t *pPayload)
{
	tBleStatus result = BLE_STATUS_INVALID_PARAMS;

	if (CharOpcode == BATTERY_LEVEL_CHAR_UUID) {
		result = aci_gatt_update_char_value(BAS_Context.SvcHdle,
				BAS_Context.BatteryLevelHandle,
				0, /* charValOffset */
				2, /* charValueLen */
				(uint8_t *)  pPayload);
	}

	return result;
}


void BAS_App_Send_Notification(uint8_t level)
{
	BAS_App_Context.Level = level;
	if (BAS_App_Context.BAS_Notification_Status) {
		BAS_Update_Char(BATTERY_LEVEL_CHAR_UUID, (uint8_t *)&(BAS_App_Context.Level));
		BAS_App_Context.Level++;
	} else {
		APP_DBG_MSG("-- BAS APPLICATION : CAN'T INFORM CLIENT -  NOTIFICATION DISABLED\n ");
	}
	return;
}

#define CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET         2
#define CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET              1

/**
 * @brief  Event handler
 * @param  Event: Address of the buffer holding the Event
 * @retval Ack: Return whether the Event has been managed or not
 */
static SVCCTL_EvtAckStatus_t BAS_Event_Handler(void *Event)
{
	SVCCTL_EvtAckStatus_t return_value;
	hci_event_pckt *event_pckt;
	evt_blecore_aci *blecore_evt;
	aci_gatt_attribute_modified_event_rp0 *attribute_modified;
	aci_gatt_write_permit_req_event_rp0   *write_perm_req;
	aci_gatt_read_permit_req_event_rp0    *read_req;
	BAS_Notification_evt_t     Notification;

	return_value = SVCCTL_EvtNotAck;
	event_pckt = (hci_event_pckt *)(((hci_uart_pckt*)Event)->data);

	switch(event_pckt->evt) {
	case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
		blecore_evt = (evt_blecore_aci*)event_pckt->data;

		// Debug
		printf("HID Event: %04X\r\n", blecore_evt->ecode);

		switch(blecore_evt->ecode) {
		case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
			attribute_modified = (aci_gatt_attribute_modified_event_rp0*)blecore_evt->data;
			if (attribute_modified->Attr_Handle == (BAS_Context.BatteryLevelHandle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;

				switch(attribute_modified->Attr_Data[0]) {
				case (!(COMSVC_Notification)):
                	Notification.BAS_Evt_Opcode = BAS_NOTIFY_DISABLED_EVT;
					BAS_Notification(&Notification);
					break;

				/* Enabled Notification management */
				case COMSVC_Notification:
					Notification.BAS_Evt_Opcode = BAS_NOTIFY_ENABLED_EVT;
					BAS_Notification(&Notification);
					break;

				default:
					break;
				}
			}

			break;

		case ACI_GATT_READ_PERMIT_REQ_VSEVT_CODE :
			read_req = (aci_gatt_read_permit_req_event_rp0*)blecore_evt->data;
			if (read_req->Attribute_Handle == (BAS_Context.BatteryLevelHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			break;

		default:
			break;
		}
		break;

	default:
		break;
	}

	return(return_value);
}


void BAS_Notification(BAS_Notification_evt_t *pNotification)
{
	switch(pNotification->BAS_Evt_Opcode) {

	case BAS_NOTIFY_ENABLED_EVT:
		BAS_App_Context.BAS_Notification_Status = 1;         // notification status has been enabled
		BAS_App_Send_Notification(BAS_App_Context.Level);
		break;

	case BAS_NOTIFY_DISABLED_EVT:
		BAS_App_Context.BAS_Notification_Status = 0;         // notification status has been disabled
		break;

	default:
		break;
	}
	return;
}
