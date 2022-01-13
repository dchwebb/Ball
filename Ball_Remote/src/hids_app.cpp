#include "common_blesvc.h"
#include "stm32_seq.h"
#include "hids_app.h"

HidService hidService;

static SVCCTL_EvtAckStatus_t HIDS_Event_Handler(void *Event);

// called from externa C prog
void HIDS_Init()
{
	hidService.Init();
}

typedef struct {
	uint16_t x;
	uint16_t y;
	uint16_t z;
} JoystickReport_t;

PLACE_IN_SECTION("BLE_APP_CONTEXT") JoystickReport_t joystickReport;








const uint8_t rep_map_data[] = {
	    // Header codes are formed of: tag (4 bits), type (2 bits), size (2 bits) [see USB_hid1_11 6.2.2.7]
	    // Eg logical maximum 0x26 = 0010 01 10 = tag: 2 (logical maximum), type: 1 (global), size: 2 bytes
	 	0x05, 0x01,			// Usage Page (Generic Desktop)
		0x09, 0x04,			// Usage (Joystick)
		0xA1, 0x01,			// Collection (Application)
		0xA1, 0x02,				// Collection (Logical)

		0x75, 0x10,					// Report size (16)
		0x95, 0x03,					// Report count (3)
		0x15, 0x00,					// Logical Minimum (0) [Logical = actual data being sent]
		0x26, 0xFF, 0x0F,			// Logical Maximum (4095)
		0x35, 0x00,					// Physical Minumum (0) [Physical = maps logical values to real unit values]
		0x46, 0xFF, 0x0F,			// Physical Maximum (4095)
		0x09, 0x30,					// Usage (X)
		0x09, 0x31,					// Usage (Y)
		0x09, 0x32,					// Usage (Z)
		0x81, 0x02,					// Input (Data, Variable, Absolute)

		0xC0,					// End collection
		0xC0				// End collection
};


void HidService::Init()
{
	uint16_t uuid;
	tBleStatus hciCmdResult = BLE_STATUS_SUCCESS;

	// Register the event handler to the BLE controller
	SVCCTL_RegisterSvcHandler(HIDS_Event_Handler);

	uuid = HUMAN_INTERFACE_DEVICE_SERVICE_UUID;
	hciCmdResult = aci_gatt_add_service(UUID_TYPE_16,
			(Service_UUID_t*) &uuid,
			PRIMARY_SERVICE,
			12,							// Max_Attribute_Records
			&(ServiceHandle));
	APP_DBG_MSG("- HIDS: Registered HID Service handle: 0x%X\n", ServiceHandle);

	// To inform the device that the host is entering or leaving suspend state
	uuid = HID_CONTROL_POINT_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(ServiceHandle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			2,							// Char value length
			CHAR_PROP_WRITE_WITHOUT_RESP,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP, // gattEvtMask
			10, 						// encryKeySize
			CHAR_VALUE_LEN_CONSTANT, 	// isVariable
			&(HidControlPointHdle));
	APP_DBG_MSG("- HIDS: Registered Control Point handle: 0x%X\n", HidControlPointHdle);

	uuid = HID_INFORMATION_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(ServiceHandle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			sizeof(HIDInformation),
			CHAR_PROP_READ,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 						// encryKeySize
			CHAR_VALUE_LEN_CONSTANT, 	// isVariable
			&(HidInformationHandle));
	APP_DBG_MSG("- HIDS: Registered HID Information characteristic handle: 0x%X\n", HidInformationHandle);


	uuid = REPORT_MAP_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(ServiceHandle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			sizeof(rep_map_data),
			CHAR_PROP_READ,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 							// encryKeySize
			CHAR_VALUE_LEN_VARIABLE,
			&(ReportMapHandle));
	APP_DBG_MSG("- HIDS: Registered Report Map characteristic handle: 0x%X\n", ReportMapHandle);

	uuid = REPORT_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(ServiceHandle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			sizeof(joystickReport),
			CHAR_PROP_READ | CHAR_PROP_WRITE | CHAR_PROP_NOTIFY,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 							// encryKeySize
			CHAR_VALUE_LEN_CONSTANT,
			&(ReportJoystickHandle));
	APP_DBG_MSG("- HIDS: Registered Report characteristic handle: 0x%X\n", ReportJoystickHandle);

	// Add char descriptor for each report
	uuid = REPORT_REFERENCE_DESCRIPTOR_UUID;
	hciCmdResult = aci_gatt_add_char_desc(ServiceHandle,
			ReportJoystickHandle,
			UUID_TYPE_16,
			(Char_Desc_Uuid_t*)&uuid,
			2,
			2,
			(uint8_t*)&JoystickReportID,		// Char_Desc_Value
			ATTR_PERMISSION_NONE,
			ATTR_ACCESS_READ_ONLY,
			GATT_DONT_NOTIFY_EVENTS,
			10,
			CHAR_VALUE_LEN_CONSTANT,
			&ReportJoystickRefDescHandle);
	APP_DBG_MSG("- HIDS: Registered Report Reference Descriptor handle: 0x%X\n", ReportJoystickRefDescHandle);

	if (hciCmdResult != BLE_STATUS_SUCCESS) {
		APP_DBG_MSG("-- HIDS APPLICATION : Error registering characteristics: 0x%X\n", hciCmdResult);
	}
}



void HidService::AppInit()
{
	JoystickNotifications = false;

	UpdateChar(ReportMap);
	UpdateChar(HidInformation);

	// Initialise the mouse movement report
	joystickReport.x = 0;
	joystickReport.y = 0;
	joystickReport.z = 0;
	UpdateChar(ReportJoystick);
}


tBleStatus HidService::UpdateChar(characteristics_t characteristic)
{
	tBleStatus result = BLE_STATUS_INVALID_PARAMS;

	if (characteristic == ReportMap) {
		result = aci_gatt_update_char_value(ServiceHandle,
				ReportMapHandle,
				0, 							// charValOffset
				sizeof(rep_map_data),		// charValueLen
				(uint8_t*)&rep_map_data);
	}
	if (characteristic == HidInformation) {
		result = aci_gatt_update_char_value(ServiceHandle,
				HidInformationHandle,
				0, 							// charValOffset
				sizeof(hidInformation),		// charValueLen
				(uint8_t*)&hidInformation);
	}
	if (characteristic == ReportJoystick) {
		result = aci_gatt_update_char_value(ServiceHandle,
				ReportJoystickHandle,
				0, 							// charValOffset
				sizeof(joystickReport),		// charValueLen
				(uint8_t*)&joystickReport);
	}

	return result;
}


void HidService::JoystickNotification(uint16_t x, uint16_t y, uint16_t z)
{
	joystickReport.x = x;
	joystickReport.y = y;
	joystickReport.z = z;

	if (JoystickNotifications) {
		UpdateChar(ReportJoystick);
	} else {
		APP_DBG_MSG("-- HIDS APPLICATION : CAN'T INFORM CLIENT -  NOTIFICATION DISABLED\n ");
	}
}




//void HIDS_Notification(HIDS_Notification_evt_t *pNotification)
//{
//	switch(pNotification->HIDS_Evt_Opcode) {
//
//	case HIDS_MOUSE_INPUT_NOTIFY_ENABLED:
//		HIDS_App_Context.HIDS_Movement_Notification_Status = 1;         // notification status has been enabled
//		HIDS_Movement_Notification(0, 0);
//		break;
//
//	case HIDS_MOUSE_INPUT_NOTIFY_DISABLED:
//		HIDS_App_Context.HIDS_Movement_Notification_Status = 0;         // notification status has been disabled
//		break;
//
//	default:
//		break;
//	}
//	return;
//}

void HIDS_ControlPointWrite(uint16_t data) {
	// Do stuff with data
	return;
}

#define CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET         2
#define CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET              1

uint16_t blankValue = 0x00;

SVCCTL_EvtAckStatus_t HidService::HIDS_Event_Handler(void *Event)
{
	SVCCTL_EvtAckStatus_t return_value;
	hci_event_pckt *event_pckt;
	evt_blecore_aci *blecore_evt;
	aci_gatt_attribute_modified_event_rp0 *attribute_modified;
	aci_gatt_write_permit_req_event_rp0   *write_perm_req;
	aci_gatt_read_permit_req_event_rp0    *read_req;
//	HIDS_Notification_evt_t     Notification;

	return_value = SVCCTL_EvtNotAck;
	event_pckt = (hci_event_pckt *)(((hci_uart_pckt*)Event)->data);

	switch (event_pckt->evt) {
	case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
		blecore_evt = (evt_blecore_aci*)event_pckt->data;

		switch(blecore_evt->ecode) {

		case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
			attribute_modified = (aci_gatt_attribute_modified_event_rp0*)blecore_evt->data;

			if (attribute_modified->Attr_Handle == (hidService.ReportJoystickHandle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;

				if (attribute_modified->Attr_Data[0] == 1) {
					hidService.JoystickNotifications = true;
					hidService.JoystickNotification(0, 0, 0);
				} else {
					hidService.JoystickNotifications = false;
				}
			}

			if (attribute_modified->Attr_Handle == (hidService.HidControlPointHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				uint16_t write_data = (attribute_modified->Attr_Data[1] << 8) | attribute_modified->Attr_Data[0];
				HIDS_ControlPointWrite(write_data);
			}

//			if (return_value == SVCCTL_EvtNotAck) {
//				return_value = SVCCTL_EvtAckFlowEnable;
//				tBleStatus result = aci_gatt_update_char_value(SvcHdle,
//						0x27,
//						0, 							// charValOffset
//						2,		// charValueLen
//						(uint8_t *)  blankValue);
//				printf("Cleared notify on handle 27: %d\r\n", result);
//			}

			break;

		case ACI_GATT_READ_PERMIT_REQ_VSEVT_CODE :
			read_req = (aci_gatt_read_permit_req_event_rp0*)blecore_evt->data;

			if (read_req->Attribute_Handle == (hidService.ReportMapHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			if (read_req->Attribute_Handle == (hidService.ReportJoystickHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			if (read_req->Attribute_Handle == (hidService.HidInformationHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			break;

		case ACI_ATT_EXEC_WRITE_RESP_VSEVT_CODE:
			write_perm_req = (aci_gatt_write_permit_req_event_rp0*)blecore_evt->data;

			break;

		case ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE:
			write_perm_req = (aci_gatt_write_permit_req_event_rp0*)blecore_evt->data;
			if (write_perm_req->Attribute_Handle == (hidService.HidControlPointHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				/* Allow or reject a write request from a client using aci_gatt_write_resp(...) function */
//				tBleStatus aci_gatt_write_resp( uint16_t Connection_Handle,
//				                                uint16_t Attr_Handle,
//				                                uint8_t Write_status,
//				                                uint8_t Error_Code,
//				                                uint8_t Attribute_Val_Length,
//				                                const uint8_t* Attribute_Val )
//				aci_gatt_write_resp(read_req->Connection_Handle);
			}
			break;


		default:
			break;
		}
		break;

	default:
		break;
	}

	return return_value;
}



