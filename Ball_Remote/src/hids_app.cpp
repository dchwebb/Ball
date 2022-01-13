#include "common_blesvc.h"
#include "stm32_seq.h"
#include "hids_app.h"

HidService hidService;

// called from external C prog svc_ctl.c
void HIDS_Init()
{
	hidService.Init();
}

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
			sizeof(reportMap),
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


tBleStatus HidService::UpdateChar(Characteristic characteristic)
{
	tBleStatus result = BLE_STATUS_INVALID_PARAMS;

	if (characteristic == ReportMap) {
		result = aci_gatt_update_char_value(ServiceHandle, ReportMapHandle,	0, sizeof(reportMap), reportMap);
	}
	if (characteristic == HidInformation) {
		result = aci_gatt_update_char_value(ServiceHandle, HidInformationHandle, 0,	sizeof(hidInformation),	(uint8_t*)&hidInformation);
	}
	if (characteristic == ReportJoystick) {
		result = aci_gatt_update_char_value(ServiceHandle, ReportJoystickHandle, 0,	sizeof(joystickReport),	(uint8_t*)&joystickReport);
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


void HidService::ControlPointWrite(uint16_t data) {
	// To inform the device that the host is entering or leaving suspend state
	return;
}


SVCCTL_EvtAckStatus_t HidService::HIDS_Event_Handler(void *Event)
{
	aci_gatt_attribute_modified_event_rp0* attribute_modified;
	aci_gatt_write_permit_req_event_rp0*   write_perm_req;
	aci_gatt_read_permit_req_event_rp0*    read_req;

	SVCCTL_EvtAckStatus_t return_value = SVCCTL_EvtNotAck;
	hci_event_pckt* event_pckt = (hci_event_pckt *)(((hci_uart_pckt*)Event)->data);

	if (event_pckt->evt ==  HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE) {
		evt_blecore_aci* blecore_evt = (evt_blecore_aci*)event_pckt->data;

		switch (blecore_evt->ecode) {

		case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
			attribute_modified = (aci_gatt_attribute_modified_event_rp0*)blecore_evt->data;

			if (attribute_modified->Attr_Handle == (hidService.ReportJoystickHandle + DescriptorOffset)) {
				return_value = SVCCTL_EvtAckFlowEnable;

				if (attribute_modified->Attr_Data[0] == 1) {
					hidService.JoystickNotifications = true;
					hidService.JoystickNotification(0, 0, 0);
				} else {
					hidService.JoystickNotifications = false;
				}
			}

			if (attribute_modified->Attr_Handle == (hidService.HidControlPointHdle + ValueOffset)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				uint16_t write_data = (attribute_modified->Attr_Data[1] << 8) | attribute_modified->Attr_Data[0];
				hidService.ControlPointWrite(write_data);
			}

			break;

		case ACI_GATT_READ_PERMIT_REQ_VSEVT_CODE:
			read_req = (aci_gatt_read_permit_req_event_rp0*)blecore_evt->data;

			if (read_req->Attribute_Handle == (hidService.ReportMapHandle + ValueOffset)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			if (read_req->Attribute_Handle == (hidService.ReportJoystickHandle + ValueOffset)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			if (read_req->Attribute_Handle == (hidService.HidInformationHandle + ValueOffset)) {
				return_value = SVCCTL_EvtAckFlowEnable;
				aci_gatt_allow_read(read_req->Connection_Handle);
			}
			break;

		case ACI_ATT_EXEC_WRITE_RESP_VSEVT_CODE:
			write_perm_req = (aci_gatt_write_permit_req_event_rp0*)blecore_evt->data;
			break;

		case ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE:
			write_perm_req = (aci_gatt_write_permit_req_event_rp0*)blecore_evt->data;
			if (write_perm_req->Attribute_Handle == (hidService.HidControlPointHdle + ValueOffset)) {
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

	}

	return return_value;
}



