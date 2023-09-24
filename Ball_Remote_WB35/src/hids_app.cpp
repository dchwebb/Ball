#include "ble.h"
#include "hids_app.h"
#include "gyroSPI.h"
#include "stm32_seq.h"

HidService hidService;

void HidService::Init()
{
	uint16_t uuid;
	tBleStatus hciCmdResult = BLE_STATUS_SUCCESS;

	uuid = HUMAN_INTERFACE_DEVICE_SERVICE_UUID;
	hciCmdResult = aci_gatt_add_service(UUID_TYPE_16,
			(Service_UUID_t*) &uuid,
			PRIMARY_SERVICE,
			16,							// Max_Attribute_Records
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

	uuid = GYRO_REGISTER_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(ServiceHandle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			4,
			CHAR_PROP_READ | CHAR_PROP_WRITE,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP | GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP, // gattEvtMask
			10, 							// encryKeySize
			CHAR_VALUE_LEN_VARIABLE,
			&(GyroRegisterHandle));
	APP_DBG_MSG("- HIDS: Registered Gyro characteristic handle: 0x%X\n", GyroRegisterHandle);

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

	AppInit();
}


void HidService::AppInit()
{
	JoystickNotifications = false;

	UpdateReportMapChar();
	UpdateHidInformationChar();

	// Initialise the joystick report
	joystickReport.x = 0;
	joystickReport.y = 0;
	joystickReport.z = 0;

	// Use the sequencer to update the report char so that to avoid write conflicts
	UTIL_SEQ_RegTask(1 << CFG_TASK_JoystickNotification, 0, UpdateJoystickReportChar);
	UTIL_SEQ_RegTask(1 << CFG_TASK_GyroNotification, 0, UpdateGyroChar);
	UpdateJoystickReportChar();
	UpdateGyroChar();
}


void HidService::UpdateJoystickReportChar()
{
	// Will trigger a notification to be sent if client is subscribed (called from sequencer)
	aci_gatt_update_char_value(
			hidService.ServiceHandle,
			hidService.ReportJoystickHandle, 0,
			sizeof(hidService.joystickReport),
			(uint8_t*)&hidService.joystickReport
			);
}


void HidService::UpdateReportMapChar()
{
	aci_gatt_update_char_value(ServiceHandle, ReportMapHandle,	0, sizeof(reportMap), reportMap);
}


void HidService::UpdateHidInformationChar()
{
	aci_gatt_update_char_value(ServiceHandle, ReportJoystickHandle, 0,	sizeof(joystickReport),	(uint8_t*)&joystickReport);
}


void HidService::UpdateGyroChar()
{
	aci_gatt_update_char_value(hidService.ServiceHandle, hidService.GyroRegisterHandle, 0, 2, (uint8_t*)&hidService.gyroRegister);
}


void HidService::JoystickNotification(int16_t x, int16_t y, int16_t z)
{
	joystickReport.x = x;
	joystickReport.y = y;
	joystickReport.z = z;

	static uint32_t lastPrint = 0;

	if (outputGyro && SysTickVal - lastPrint > 300) {
		printf("x: %d y: %d z: %d\r\n", x, y, z);
		lastPrint = SysTickVal;
	}

	UTIL_SEQ_SetTask(1 << CFG_TASK_JoystickNotification, CFG_SCH_PRIO_0);
}


void HidService::ControlPointWrite(uint16_t data) {
	// To inform the device that the host is entering or leaving suspend state
	return;
}


void HidService::Disconnect() {
	JoystickNotifications = false;
	gyro.ContinualRead(false);
}


bool HidService::EventHandler(hci_event_pckt* event_pckt)
{
	aci_gatt_attribute_modified_event_rp0* attribute_modified;
	aci_gatt_write_permit_req_event_rp0*   write_perm_req;
	aci_gatt_read_permit_req_event_rp0*    read_req;

	bool handled = false;

	evt_blecore_aci* blecore_evt = (evt_blecore_aci*)event_pckt->data;

	switch (blecore_evt->ecode) {

	case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
		attribute_modified = (aci_gatt_attribute_modified_event_rp0*)blecore_evt->data;

		if (attribute_modified->Attr_Handle == (hidService.ReportJoystickHandle + DescriptorOffset)) {
			handled = true;

			if (attribute_modified->Attr_Data[0] == 1) {
				hidService.JoystickNotifications = true;
				gyro.ContinualRead(true);
			} else {
				hidService.JoystickNotifications = false;
				gyro.ContinualRead(false);
			}
		}

		if (attribute_modified->Attr_Handle == (hidService.HidControlPointHdle + ValueOffset)) {
			handled = true;
			uint16_t write_data = (attribute_modified->Attr_Data[1] << 8) | attribute_modified->Attr_Data[0];
			hidService.ControlPointWrite(write_data);
		}

		if (attribute_modified->Attr_Handle == (hidService.GyroRegisterHandle + ValueOffset)) {
			// Provide mechanism to read and write gyro registers. If one byte sent then the value of that register can be read; two bytes to write to a register
			gyroRegister.reg = attribute_modified->Attr_Data[0];			// Store register number in case reading
			if (attribute_modified->Attr_Data_Length == 1) {
				if (gyroRegister.reg >= 0x0F && gyroRegister.reg <= 0x38) {
					gyroRegister.val = gyro.ReadRegister(gyroRegister.reg);
					UTIL_SEQ_SetTask(1 << CFG_TASK_GyroNotification, CFG_SCH_PRIO_0);
				}
			} else {
				gyroRegister.val = attribute_modified->Attr_Data[1];
				gyro.WriteCmd(gyroRegister.reg, gyroRegister.val);
			}

			handled = true;
		}

		break;

	case ACI_GATT_READ_PERMIT_REQ_VSEVT_CODE:
		read_req = (aci_gatt_read_permit_req_event_rp0*)blecore_evt->data;

		if (read_req->Attribute_Handle == (hidService.ReportMapHandle + ValueOffset)) {
			handled = true;
			aci_gatt_allow_read(read_req->Connection_Handle);
		}
		if (read_req->Attribute_Handle == (hidService.ReportJoystickHandle + ValueOffset)) {
			aci_gatt_allow_read(read_req->Connection_Handle);
			handled = true;
			gyro.GyroRead();
			JoystickNotification(gyro.gyroData.x, gyro.gyroData.y, gyro.gyroData.z);
		}
		if (read_req->Attribute_Handle == (hidService.HidInformationHandle + ValueOffset)) {
			handled = true;
			aci_gatt_allow_read(read_req->Connection_Handle);
		}
		if (read_req->Attribute_Handle == (hidService.GyroRegisterHandle + ValueOffset)) {
			handled = true;
			aci_gatt_allow_read(read_req->Connection_Handle);
		}
		break;

	case ACI_ATT_EXEC_WRITE_RESP_VSEVT_CODE:
		write_perm_req = (aci_gatt_write_permit_req_event_rp0*)blecore_evt->data;
		break;

	case ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE:
		write_perm_req = (aci_gatt_write_permit_req_event_rp0*)blecore_evt->data;
		if (write_perm_req->Attribute_Handle == (hidService.HidControlPointHdle + ValueOffset)) {
			handled = true;
		}
//		if (write_perm_req->Attribute_Handle == (hidService.GyroRegisterHandle + ValueOffset)) {
//			uint8_t len = write_perm_req->Data_Length;
//			gyroRegister = write_perm_req->Data[0];			// Store register number in case reading
//			if (len == 2) {
//				uint8_t val = write_perm_req->Data[1];
//			}
//
//			handled = true;
//		}
		break;

	default:
		break;
	}


	return handled;
}



