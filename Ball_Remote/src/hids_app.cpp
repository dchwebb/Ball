#include "common_blesvc.h"
#include "stm32_seq.h"
#include "hids_app.h"

static SVCCTL_EvtAckStatus_t HIDS_Event_Handler(void *Event);


typedef struct
{
	uint16_t SvcHdle;				 	// Service handle
	uint16_t ReportJoystickHandle;
	uint16_t ReportJoystickRefDescHandle;
	uint16_t HidInformationHandle;
	uint16_t HidControlPointHdle;
	uint16_t ReportMapHandle;
//	uint16_t ProtocolModeHandle;
} HIDS_Context_t;

PLACE_IN_SECTION("BLE_DRIVER_CONTEXT") static HIDS_Context_t HIDS_Context;

typedef struct {
	uint8_t   HIDS_Movement_Notification_Status;
	uint8_t   HIDS_MPlayer_Notification_Status;
	uint8_t HIDS_Buttons_Notification_Status;
} HIDS_APP_Context_t;

PLACE_IN_SECTION("BLE_APP_CONTEXT") HIDS_APP_Context_t HIDS_App_Context;

typedef struct {
	uint16_t x;
	uint16_t y;
	uint16_t z;
} JoystickReport_t;

PLACE_IN_SECTION("BLE_APP_CONTEXT") JoystickReport_t joystickReport;

// First Byte: Input Report (0x01) | Second Byte: Report ID (0x1 ... 0x03)
static const uint16_t JoystickReportID  = 0x0101;

//PLACE_IN_SECTION("BLE_APP_CONTEXT") uint8_t protocolModeData;		// 0 = boot mode; 1 = report mode

#define HID_INFO_FLAG_REMOTE_WAKE_MSK           0x01
#define HID_INFO_FLAG_NORMALLY_CONNECTABLE_MSK  0x02

typedef struct {
	uint16_t bcdHID;         // 16-bit unsigned integer representing version number of base USB HID Specification implemented by HID Device
	uint8_t  bcountryCode;   // Identifies which country the hardware is localized for. Most hardware is not localized and thus this value would be zero (0).
	uint8_t  flags;          // See http://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.hid_information.xml
} HID_Information_t;

PLACE_IN_SECTION("BLE_APP_CONTEXT") HID_Information_t HID_Information;

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


void HIDS_Init()
{
	uint16_t uuid;
	tBleStatus hciCmdResult = BLE_STATUS_SUCCESS;

	// Register the event handler to the BLE controller
	SVCCTL_RegisterSvcHandler(HIDS_Event_Handler);

	uuid = HUMAN_INTERFACE_DEVICE_SERVICE_UUID;
	hciCmdResult = aci_gatt_add_service(UUID_TYPE_16,
			(Service_UUID_t*) &uuid,
			PRIMARY_SERVICE,
			24,		// Max_Attribute_Records
			&(HIDS_Context.SvcHdle));

	uuid = HID_CONTROL_POINT_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(HIDS_Context.SvcHdle,
			UUID_TYPE_16,
			(Char_UUID_t*) &uuid,
			2,							// Char value length
			CHAR_PROP_WRITE_WITHOUT_RESP,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP, // gattEvtMask
			10, 						// encryKeySize
			CHAR_VALUE_LEN_CONSTANT, 	// isVariable
			&(HIDS_Context.HidControlPointHdle));


	uuid = HID_INFORMATION_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(HIDS_Context.SvcHdle,
			UUID_TYPE_16,
			(Char_UUID_t*) &uuid,
			4,
			CHAR_PROP_READ,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 						// encryKeySize
			CHAR_VALUE_LEN_CONSTANT, 	// isVariable
			&(HIDS_Context.HidInformationHandle));

	uuid = REPORT_MAP_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(HIDS_Context.SvcHdle,
			UUID_TYPE_16,
			(Char_UUID_t *) &uuid,
			sizeof rep_map_data,
			CHAR_PROP_READ,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 							// encryKeySize
			CHAR_VALUE_LEN_VARIABLE,
			&(HIDS_Context.ReportMapHandle));

	uuid = REPORT_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(HIDS_Context.SvcHdle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			sizeof joystickReport,
			CHAR_PROP_READ | CHAR_PROP_WRITE | CHAR_PROP_NOTIFY,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 							// encryKeySize
			CHAR_VALUE_LEN_CONSTANT,
			&(HIDS_Context.ReportJoystickHandle));

	// Add char descriptor for each report?
	uuid = REPORT_REFERENCE_DESCRIPTOR_UUID;
	hciCmdResult = aci_gatt_add_char_desc(HIDS_Context.SvcHdle,
			HIDS_Context.ReportJoystickHandle,
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
			&HIDS_Context.ReportJoystickRefDescHandle);

//	uuid = PROTOCOL_MODE_CHAR_UUID;
//	hciCmdResult = aci_gatt_add_char(HIDS_Context.SvcHdle,
//			UUID_TYPE_16,
//			(Char_UUID_t *) &uuid,
//			1,
//			CHAR_PROP_READ | CHAR_PROP_WRITE_WITHOUT_RESP,
//			ATTR_PERMISSION_NONE,
//			GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
//			10, 							// encryKeySize
//			CHAR_VALUE_LEN_CONSTANT,
//			&(HIDS_Context.ProtocolModeHandle));

}



void HIDS_App_Init()
{
	// Initialise notification info
	HIDS_App_Context.HIDS_MPlayer_Notification_Status = 0;
	HIDS_App_Context.HIDS_Movement_Notification_Status = 0;

	// Initialize Report Map
	HIDS_Update_Char(ReportMap, 0, 0, (uint8_t*)&rep_map_data);

	// Initialise the HID Information data
	HID_Information.bcdHID = 0x0101;			// Binary coded decimal HID version
	HID_Information.bcountryCode = 0x0;			// Country code
	HID_Information.flags =  HID_INFO_FLAG_REMOTE_WAKE_MSK | HID_INFO_FLAG_NORMALLY_CONNECTABLE_MSK;
	HIDS_Update_Char(HidInformation, 0, 0, (uint8_t*)&HID_Information);

	// Initialise the mouse movement report
	joystickReport.x = 0;
	joystickReport.y = 0;
	joystickReport.z = 0;
	HIDS_Update_Char(ReportJoystick, 0, 0, (uint8_t*)&joystickReport);

	// Initialise protocol mode (for selecting between boot mouse and regular)
//	protocolModeData = 0x01;
//	HIDS_Update_Char(ProtocolMode, 0, 0, (uint8_t*)&protocolModeData);

	return;
}



tBleStatus HIDS_Update_Char(characteristics_t characteristic, uint8_t Report_Index, uint8_t report_size, uint8_t *pPayload)
{
	tBleStatus result = BLE_STATUS_INVALID_PARAMS;

	if (characteristic == ReportMap) {
		result = aci_gatt_update_char_value(HIDS_Context.SvcHdle,
				HIDS_Context.ReportMapHandle,
				0, 						// charValOffset
				sizeof rep_map_data,	// charValueLen
				pPayload);
	}
	if (characteristic == HidInformation) {
		result = aci_gatt_update_char_value(HIDS_Context.SvcHdle,
				HIDS_Context.HidInformationHandle,
				0, 							// charValOffset
				sizeof HID_Information,		// charValueLen
				pPayload);
	}
	if (characteristic == ReportJoystick) {
		result = aci_gatt_update_char_value(HIDS_Context.SvcHdle,
				HIDS_Context.ReportJoystickHandle,
				0, 							// charValOffset
				sizeof joystickReport,		// charValueLen
				pPayload);
	}
//
//	if (characteristic == ProtocolMode) {
//		result = aci_gatt_update_char_value(HIDS_Context.SvcHdle,
//				HIDS_Context.ProtocolModeHandle,
//				0, 							// charValOffset
//				sizeof protocolModeData,		// charValueLen
//				pPayload);
//	}
//
	return result;
}



void HIDS_Joystick_Notification(uint16_t x, uint16_t y, uint16_t z)
{
	joystickReport.x = x;
	joystickReport.y = y;
	joystickReport.z = z;

	if (HIDS_App_Context.HIDS_Movement_Notification_Status) {
		HIDS_Update_Char(ReportJoystick, 0, 0, (uint8_t*)&joystickReport);
	} else {
		APP_DBG_MSG("-- HIDS APPLICATION : CAN'T INFORM CLIENT -  NOTIFICATION DISABLED\n ");
	}
	return;
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

static SVCCTL_EvtAckStatus_t HIDS_Event_Handler(void *Event)
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

		// Debug
//		printf("HID Event: %04X\r\n", blecore_evt->ecode);

		switch(blecore_evt->ecode) {

		case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
			attribute_modified = (aci_gatt_attribute_modified_event_rp0*)blecore_evt->data;

			if (attribute_modified->Attr_Handle == (HIDS_Context.ReportJoystickHandle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;

				if (attribute_modified->Attr_Data[0] == 1) {
					HIDS_App_Context.HIDS_Movement_Notification_Status = 1;
					HIDS_Joystick_Notification(0, 0, 0);
				} else {
  					HIDS_App_Context.HIDS_Movement_Notification_Status = 0;
				}
			}

			if (attribute_modified->Attr_Handle == (HIDS_Context.HidControlPointHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				uint16_t write_data = (attribute_modified->Attr_Data[1] << 8) | attribute_modified->Attr_Data[0];
				HIDS_ControlPointWrite(write_data);
			}

//			if (return_value == SVCCTL_EvtNotAck) {
//				return_value = SVCCTL_EvtAckFlowEnable;
//				tBleStatus result = aci_gatt_update_char_value(HIDS_Context.SvcHdle,
//						0x27,
//						0, 							// charValOffset
//						2,		// charValueLen
//						(uint8_t *)  blankValue);
//				printf("Cleared notify on handle 27: %d\r\n", result);
//			}

			break;

		case ACI_GATT_READ_PERMIT_REQ_VSEVT_CODE :
			read_req = (aci_gatt_read_permit_req_event_rp0*)blecore_evt->data;

			if (read_req->Attribute_Handle == (HIDS_Context.ReportMapHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			if (read_req->Attribute_Handle == (HIDS_Context.ReportJoystickHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			if (read_req->Attribute_Handle == (HIDS_Context.HidInformationHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
//			if (read_req->Attribute_Handle == (HIDS_Context.ProtocolModeHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
//				return_value = SVCCTL_EvtAckFlowEnable;
//				aci_gatt_allow_read(read_req->Connection_Handle);
//			}
			break;

		case ACI_ATT_EXEC_WRITE_RESP_VSEVT_CODE:
			write_perm_req = (aci_gatt_write_permit_req_event_rp0*)blecore_evt->data;

			break;

		case ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE:
			write_perm_req = (aci_gatt_write_permit_req_event_rp0*)blecore_evt->data;
			if (write_perm_req->Attribute_Handle == (HIDS_Context.HidControlPointHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
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



