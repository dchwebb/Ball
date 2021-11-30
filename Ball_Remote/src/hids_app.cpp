#include "common_blesvc.h"
#include "stm32_seq.h"

static SVCCTL_EvtAckStatus_t HIDS_Event_Handler(void *Event);


typedef struct
{
	uint16_t SvcHdle;				 	// Service handle
	uint16_t ReportButtonsHandle;		// Characteristic handles
	uint16_t ReportButtonsRefDescHandle;
	uint16_t ReportMovementHandle;
	uint16_t ReportMovementRefDescHandle;
	uint16_t ReportMPlayerHandle;
	uint16_t ReportMPlayerRefDescHandle;
	uint16_t HidInformationHandle;
	uint16_t HidControlPointHdle;
	uint16_t BootMouseInputHandle;
	uint16_t ReportMapHandle;
	uint16_t ProtocolModeHandle;

} HIDS_Context_t;

PLACE_IN_SECTION("BLE_DRIVER_CONTEXT") static HIDS_Context_t HIDS_Context;

typedef struct {
	uint8_t   HIDS_Movement_Notification_Status;
	uint8_t   HIDS_MPlayer_Notification_Status;
	uint8_t HIDS_Buttons_Notification_Status;
} HIDS_APP_Context_t;

PLACE_IN_SECTION("BLE_APP_CONTEXT") HIDS_APP_Context_t HIDS_App_Context;

typedef struct {
	uint8_t buttons;
	uint8_t x_displacement;
	uint8_t y_displacement;
	uint8_t device_specific[5];
} Boot_Mouse_Report_t;

PLACE_IN_SECTION("BLE_APP_CONTEXT") Boot_Mouse_Report_t Boot_Mouse_Report;

typedef struct {
	uint8_t buttons;		// Only five bits used
	uint8_t wheel;
	uint8_t ACPan;
} Mouse_Buttons_Report_t;

PLACE_IN_SECTION("BLE_APP_CONTEXT") Mouse_Buttons_Report_t Mouse_Buttons_Report;

typedef struct {
	uint32_t x : 12, y : 12, : 8;
} Mouse_Movement_Report_t;

PLACE_IN_SECTION("BLE_APP_CONTEXT") Mouse_Movement_Report_t Mouse_Movement_Report;

typedef struct {
	uint8_t mediaData;
} Mouse_MPlayer_Report_t;

PLACE_IN_SECTION("BLE_APP_CONTEXT") Mouse_MPlayer_Report_t Mouse_MPlayer_Report;

PLACE_IN_SECTION("BLE_APP_CONTEXT") uint8_t protocolModeData;		// 0 = boot mode; 1 = report mode

#define HID_INFO_FLAG_REMOTE_WAKE_MSK           0x01
#define HID_INFO_FLAG_NORMALLY_CONNECTABLE_MSK  0x02

typedef struct {
	uint16_t bcdHID;         // 16-bit unsigned integer representing version number of base USB HID Specification implemented by HID Device
	uint8_t  bcountryCode;   // Identifies which country the hardware is localized for. Most hardware is not localized and thus this value would be zero (0).
	uint8_t  flags;          // See http://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.hid_information.xml
} HID_Information_t;

PLACE_IN_SECTION("BLE_APP_CONTEXT") HID_Information_t HID_Information;

const uint8_t rep_map_data_js[] =
{
	0x05, 0x01,			//Usage Page (Generic Desktop)
	0x09, 0x04,			//Usage (Joystick)
	0xA1, 0x01,			//Collection (Application)
	0xA1, 0x02,				//Collection (Logical)
	0x75, 0x08,					//Report size (8)
	0x95, 0x04,					//Report count (4)
	0x15, 0x00,					//Logical Minimum (-127)
	0x26, 0xFF, 0x00,			//Logical Maximum (128)
	0x35, 0x00,					//Physical Minumum (0)
	0x46, 0xFF, 0x00,			//Physical Maximum (255)
	0x09, 0x30,					//Usage (X)
	0x09, 0x31,					//Usage (Y)
	0x09, 0x32,					//Usage (Z)
	0x09, 0x35,					//Usage (Rz)
	0x81, 0x02,					//Input (Data, Variable, Absolute)
	0x75, 0x08,					//Report size (8)
	0x95, 0x01,					//Report count (1)
	0x25, 0x07,					//Logical Maximum (7)
	0x46, 0x3B, 0x01,			//Physical Maximum (315)
	0x65, 0x14,					//Unit (Eng Rot: Degree)
	0x09, 0x39,					//Usage (Hat switch)
	0x81, 0x42,					//Input (Data, Variable, Absolute, Null)
	0x65, 0x00,					//Unit (None)
	0x75, 0x01,					//Report size (1)
	0x95, 0x08,					//Report count (8)
	0x25, 0x01,					//Logical Maximum (1)
	0x45, 0x01,					//Physical Maximum (1)
	0x05, 0x09,					//Usage Page (Button)
	0x19, 0x01,					//Usage Minumum (Button 1)
	0x29, 0x08,					//Usage Maximum (Button 8)
	0x81, 0x02,					//Input (Data, Variable, Relative)
	0xC0,					//End collection
	0xC0				//End collection

};

const uint8_t rep_map_data[] =
 {
     0x05, 0x01,       // Usage Page (Generic Desktop)
     0x09, 0x02,       // Usage (Mouse)

     0xA1, 0x01,       // Collection (Application)

     // Report ID 1: Mouse buttons + scroll/pan [5 bits buttons + 3 padding | 8 bits wheel | 8 bits AC Pan]
     0x85, 0x01,       // Report Id 1
     0x09, 0x01,       // Usage (Pointer)
     0xA1, 0x00,       // Collection (Physical)

     0x95, 0x05,       // Report Count (5)
     0x75, 0x01,       // Report Size (1) bits
     0x05, 0x09,       // Usage Page (Buttons)
     0x19, 0x01,       // Usage Minimum (01)
     0x29, 0x05,       // Usage Maximum (05)
     0x15, 0x00,       // Logical Minimum (0)
     0x25, 0x01,       // Logical Maximum (1)
     0x81, 0x02,       // Input (Data, Variable, Absolute)

     0x95, 0x01,       // Report Count (1)
     0x75, 0x03,       // Report Size (3) bits
     0x81, 0x01,       // Input (Constant) for padding

	 0x75, 0x08,       // Report Size (8) bits
     0x95, 0x01,       // Report Count (1)
     0x05, 0x01,       // Usage Page (Generic Desktop)
     0x09, 0x38,       // Usage (Wheel)
     0x15, 0x81,       // Logical Minimum (-127)
     0x25, 0x7F,       // Logical Maximum (127)
     0x81, 0x06,       // Input (Data, Variable, Relative)

	 0x05, 0x0C,       // Usage Page (Consumer)
     0x0A, 0x38, 0x02, // Usage (AC Pan)
     0x95, 0x01,       // Report Count (1)
     0x81, 0x06,       // Input (Data,Value,Relative,Bit Field)

	 0xC0,             // End Collection (Physical)

     // Report ID 2: Mouse motion
     0x85, 0x02,       // Report Id 2
     0x09, 0x01,       // Usage (Pointer)
     0xA1, 0x00,       // Collection (Physical)
     0x75, 0x0C,       // Report Size (12) bits?
     0x95, 0x02,       // Report Count (2) total size is 2 x 12 bits = 3 bytes
     0x05, 0x01,       // Usage Page (Generic Desktop)
     0x09, 0x30,       // Usage (X)
     0x09, 0x31,       // Usage (Y)
     0x16, 0x01, 0xF8, // Logical maximum (2047)
     0x26, 0xFF, 0x07, // Logical minimum (-2047)
     0x81, 0x06,       // Input (Data, Variable, Relative)
     0xC0,             // End Collection (Physical)
     0xC0,             // End Collection (Application)

     // Report ID 3: Advanced buttons [8 bits for media control, application launch]
     0x05, 0x0C,       // Usage Page (Consumer)
     0x09, 0x01,       // Usage (Consumer Control)
     0xA1, 0x01,       // Collection (Application)
     0x85, 0x03,       // Report Id (3)
     0x15, 0x00,       // Logical minimum (0)
     0x25, 0x01,       // Logical maximum (1)
     0x75, 0x01,       // Report Size (1)
     0x95, 0x01,       // Report Count (1)

     0x09, 0xCD,       // Usage (Play/Pause)
     0x81, 0x06,       // Input (Data,Value,Relative,Bit Field)
     0x0A, 0x83, 0x01, // Usage (AL Consumer Control Configuration)
     0x81, 0x06,       // Input (Data,Value,Relative,Bit Field)
     0x09, 0xB5,       // Usage (Scan Next Track)
     0x81, 0x06,       // Input (Data,Value,Relative,Bit Field)
     0x09, 0xB6,       // Usage (Scan Previous Track)
     0x81, 0x06,       // Input (Data,Value,Relative,Bit Field)
     0x09, 0xEA,       // Usage (Volume Down)
     0x81, 0x06,       // Input (Data,Value,Relative,Bit Field)
     0x09, 0xE9,       // Usage (Volume Up)
     0x81, 0x06,       // Input (Data,Value,Relative,Bit Field)
     0x0A, 0x25, 0x02, // Usage (AC Forward)
     0x81, 0x06,       // Input (Data,Value,Relative,Bit Field)
     0x0A, 0x24, 0x02, // Usage (AC Back)
     0x81, 0x06,       // Input (Data,Value,Relative,Bit Field)
     0xC0              // End Collection
 };

// First Byte: Input Report (0x01) | Second Byte: Report ID (0x1 ... 0x03)
static const uint16_t ButtonsReport  = 0x0101;
static const uint16_t MovementReport = 0x0102;
static const uint16_t MPlayerReport = 0x0103;

void HIDS_Init(void)
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

	// Not needed for joystick
	uuid = BOOT_MOUSE_INPUT_REPORT_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(HIDS_Context.SvcHdle,
			UUID_TYPE_16,
			(Char_UUID_t *) &uuid,
			8,
			CHAR_PROP_READ | CHAR_PROP_WRITE | CHAR_PROP_NOTIFY,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 							// encryKeySize
			CHAR_VALUE_LEN_VARIABLE, 		// isVariable
			&(HIDS_Context.BootMouseInputHandle));

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
			3,
			CHAR_PROP_READ | CHAR_PROP_WRITE | CHAR_PROP_NOTIFY,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 							// encryKeySize
			CHAR_VALUE_LEN_CONSTANT,
			&(HIDS_Context.ReportButtonsHandle));

	// Add char descriptor for each report?
	uuid = REPORT_REFERENCE_DESCRIPTOR_UUID;
	hciCmdResult = aci_gatt_add_char_desc(HIDS_Context.SvcHdle,
			HIDS_Context.ReportButtonsHandle,
			UUID_TYPE_16,
			(Char_Desc_Uuid_t*)&uuid,
			2,
			2,
			(uint8_t*)&ButtonsReport,		// Char_Desc_Value
			ATTR_PERMISSION_NONE,
			ATTR_ACCESS_READ_ONLY,
			GATT_DONT_NOTIFY_EVENTS,
			10,
			CHAR_VALUE_LEN_CONSTANT,
			&HIDS_Context.ReportButtonsRefDescHandle);

	uuid = REPORT_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(HIDS_Context.SvcHdle,
			UUID_TYPE_16,
			(Char_UUID_t *) &uuid,
			2,
			CHAR_PROP_READ | CHAR_PROP_WRITE | CHAR_PROP_NOTIFY,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 							// encryKeySize
			CHAR_VALUE_LEN_CONSTANT,
			&(HIDS_Context.ReportMovementHandle));

	// Add char descriptor for each report?
	uuid = REPORT_REFERENCE_DESCRIPTOR_UUID;
	hciCmdResult = aci_gatt_add_char_desc(HIDS_Context.SvcHdle,
			HIDS_Context.ReportMovementHandle,
			UUID_TYPE_16,
			(Char_Desc_Uuid_t*)&uuid,
			2,
			2,
			(uint8_t*)&MovementReport,		// Char_Desc_Value
			ATTR_PERMISSION_NONE,
			ATTR_ACCESS_READ_ONLY,
			GATT_DONT_NOTIFY_EVENTS,
			10,
			CHAR_VALUE_LEN_CONSTANT,
			&HIDS_Context.ReportMovementRefDescHandle);


	uuid = REPORT_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(HIDS_Context.SvcHdle,
			UUID_TYPE_16,
			(Char_UUID_t *) &uuid,
			1,
			CHAR_PROP_READ | CHAR_PROP_WRITE | CHAR_PROP_NOTIFY,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 							// encryKeySize
			CHAR_VALUE_LEN_CONSTANT,
			&(HIDS_Context.ReportMPlayerHandle));

	uuid = REPORT_REFERENCE_DESCRIPTOR_UUID;
	hciCmdResult = aci_gatt_add_char_desc(HIDS_Context.SvcHdle,
			HIDS_Context.ReportMPlayerHandle,
			UUID_TYPE_16,
			(Char_Desc_Uuid_t*)&uuid,
			2,
			2,
			(uint8_t*)&MPlayerReport,		// Char_Desc_Value
			ATTR_PERMISSION_NONE,
			ATTR_ACCESS_READ_ONLY,
			GATT_DONT_NOTIFY_EVENTS,
			10,
			CHAR_VALUE_LEN_CONSTANT,
			&HIDS_Context.ReportMPlayerRefDescHandle);


	uuid = PROTOCOL_MODE_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(HIDS_Context.SvcHdle,
			UUID_TYPE_16,
			(Char_UUID_t *) &uuid,
			1,
			CHAR_PROP_READ | CHAR_PROP_WRITE_WITHOUT_RESP,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 							// encryKeySize
			CHAR_VALUE_LEN_CONSTANT,
			&(HIDS_Context.ProtocolModeHandle));

}



void HIDS_App_Init()
{
	// Initialise notification info
	HIDS_App_Context.HIDS_MPlayer_Notification_Status = 0;
	HIDS_App_Context.HIDS_Movement_Notification_Status = 0;

	// Initialize Report Map
	HIDS_Update_Char(ReportMap, 0, 0, (uint8_t*)&rep_map_data);

	// initialise boot mouse report
	Boot_Mouse_Report.buttons = 0;
	Boot_Mouse_Report.x_displacement = 0;
	Boot_Mouse_Report.y_displacement = 0;
	HIDS_Update_Char(BootMouseInput, 0, 0, (uint8_t*)&Boot_Mouse_Report);

	// Initialise the HID Information data
	HID_Information.bcdHID = 0x0101;			// Binary coded decimal HID version
	HID_Information.bcountryCode = 0x0;			// Country code
	HID_Information.flags =  HID_INFO_FLAG_REMOTE_WAKE_MSK | HID_INFO_FLAG_NORMALLY_CONNECTABLE_MSK;
	HIDS_Update_Char(HidInformation, 0, 0, (uint8_t*)&HID_Information);

	// Initialise the mouse movement report
	Mouse_Movement_Report.x = 0;
	Mouse_Movement_Report.y = 0;
	HIDS_Update_Char(ReportMovement, 0, 0, (uint8_t*)&Mouse_Movement_Report);

	// Initialise the mouse media player report
	Mouse_MPlayer_Report.mediaData = 0x0;
	HIDS_Update_Char(ReportMPlayer, 0, 0, (uint8_t*)&Mouse_MPlayer_Report);

	// Initialise the mouse buttons report
	Mouse_Buttons_Report.buttons = 0x0;
	Mouse_Buttons_Report.wheel = 0x0;
	Mouse_Buttons_Report.ACPan = 0x0;
	HIDS_Update_Char(ReportButtons, 0, 0, (uint8_t*)&Mouse_Buttons_Report);

	// Initialise protocol mode (for selecting between boot mouse and regular)
	protocolModeData = 0x01;
	HIDS_Update_Char(ProtocolMode, 0, 0, (uint8_t*)&protocolModeData);

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
	if (characteristic == BootMouseInput) {
		result = aci_gatt_update_char_value(HIDS_Context.SvcHdle,
				HIDS_Context.BootMouseInputHandle,
				0, 							// charValOffset
				sizeof Boot_Mouse_Report,	// charValueLen
				pPayload);
	}
	if (characteristic == HidInformation) {
		result = aci_gatt_update_char_value(HIDS_Context.SvcHdle,
				HIDS_Context.HidInformationHandle,
				0, 							// charValOffset
				sizeof HID_Information,		// charValueLen
				pPayload);
	}
	if (characteristic == ReportMovement) {
		result = aci_gatt_update_char_value(HIDS_Context.SvcHdle,
				HIDS_Context.ReportMovementHandle,
				0, 							// charValOffset
				sizeof Mouse_Movement_Report,		// charValueLen
				pPayload);
	}
	if (characteristic == ReportMPlayer) {
		result = aci_gatt_update_char_value(HIDS_Context.SvcHdle,
				HIDS_Context.ReportMPlayerHandle,
				0, 							// charValOffset
				sizeof Mouse_MPlayer_Report,		// charValueLen
				 pPayload);
	}
	if (characteristic == ReportButtons) {
		result = aci_gatt_update_char_value(HIDS_Context.SvcHdle,
				HIDS_Context.ReportButtonsHandle,
				0, 							// charValOffset
				sizeof Mouse_Buttons_Report,		// charValueLen
				pPayload);
	}
	if (characteristic == ProtocolMode) {
		result = aci_gatt_update_char_value(HIDS_Context.SvcHdle,
				HIDS_Context.ProtocolModeHandle,
				0, 							// charValOffset
				sizeof protocolModeData,		// charValueLen
				pPayload);
	}	return result;
}


void HIDS_Buttons_Notification(uint8_t buttons, uint8_t wheel, uint8_t ACPan)
{
	Mouse_Buttons_Report.buttons = buttons;
	Mouse_Buttons_Report.wheel = wheel;
	Mouse_Buttons_Report.ACPan = ACPan;

	if (HIDS_App_Context.HIDS_Buttons_Notification_Status) {
		HIDS_Update_Char(ReportButtons, 0, 0, (uint8_t*)&Mouse_Buttons_Report);
	} else {
		APP_DBG_MSG("-- HIDS APPLICATION : CAN'T INFORM CLIENT -  NOTIFICATION DISABLED\n ");
	}
	return;
}


void HIDS_Movement_Notification(uint16_t x, uint16_t y)
{
	Mouse_Movement_Report.x = x;
	Mouse_Movement_Report.y = y;

	if (HIDS_App_Context.HIDS_Movement_Notification_Status) {
		HIDS_Update_Char(ReportMovement, 0, 0, (uint8_t*)&Mouse_Movement_Report);
	} else {
		APP_DBG_MSG("-- HIDS APPLICATION : CAN'T INFORM CLIENT -  NOTIFICATION DISABLED\n ");
	}
	return;
}


void HIDS_MPlayer_Notification(uint8_t mediaData)
{
	Mouse_MPlayer_Report.mediaData = mediaData;

	if (HIDS_App_Context.HIDS_MPlayer_Notification_Status) {
		HIDS_Update_Char(ReportMPlayer, 0, 0, (uint8_t*)&Mouse_MPlayer_Report);
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

			if (attribute_modified->Attr_Handle == (HIDS_Context.ReportMovementHandle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;

				if (attribute_modified->Attr_Data[0] == 1) {
					HIDS_App_Context.HIDS_Movement_Notification_Status = 1;
					HIDS_Movement_Notification(0, 0);
				} else {
  					HIDS_App_Context.HIDS_Movement_Notification_Status = 0;
				}
			}

			if (attribute_modified->Attr_Handle == (HIDS_Context.ReportMPlayerHandle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;

				if (attribute_modified->Attr_Data[0] == 1) {
					HIDS_App_Context.HIDS_MPlayer_Notification_Status = 1;
					HIDS_MPlayer_Notification(0xEF);
				} else {
					HIDS_App_Context.HIDS_MPlayer_Notification_Status = 0;
				}
			}

			if (attribute_modified->Attr_Handle == (HIDS_Context.ReportButtonsHandle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;

				if (attribute_modified->Attr_Data[0] == 1) {
					HIDS_App_Context.HIDS_Buttons_Notification_Status = 1;
					HIDS_Buttons_Notification(0x0, 0x0, 0x0);
				} else {
					HIDS_App_Context.HIDS_Buttons_Notification_Status = 0;
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
			if (read_req->Attribute_Handle == (HIDS_Context.ReportButtonsHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			if (read_req->Attribute_Handle == (HIDS_Context.ReportMapHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			if (read_req->Attribute_Handle == (HIDS_Context.ReportButtonsHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			if (read_req->Attribute_Handle == (HIDS_Context.ReportMovementHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			if (read_req->Attribute_Handle == (HIDS_Context.ReportMPlayerHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			if (read_req->Attribute_Handle == (HIDS_Context.BootMouseInputHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			if (read_req->Attribute_Handle == (HIDS_Context.HidInformationHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			if (read_req->Attribute_Handle == (HIDS_Context.ProtocolModeHandle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
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



