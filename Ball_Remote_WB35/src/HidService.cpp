#include "HidService.h"
#include "ble.h"
#include "gyroSPI.h"
#include "stm32_seq.h"
#include "BleApp.h"
#include "configManager.h"
#include <algorithm>


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
			&(serviceHandle));
	printf("- HIDS: Registered HID Service handle: 0x%X\n", serviceHandle);

	// To inform the device that the host is entering or leaving suspend state
	uuid = HID_CONTROL_POINT_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(serviceHandle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			2,							// Char value length
			CHAR_PROP_WRITE_WITHOUT_RESP,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP, // gattEvtMask
			10, 						// encryKeySize
			CHAR_VALUE_LEN_CONSTANT, 	// isVariable
			&(hidControlPointHandle));
	printf("- HIDS: Registered Control Point handle: 0x%X\n", hidControlPointHandle);

	uuid = HID_INFORMATION_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(serviceHandle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			sizeof(HIDInformation),
			CHAR_PROP_READ,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 						// encryKeySize
			CHAR_VALUE_LEN_CONSTANT, 	// isVariable
			&(hidInformationHandle));
	printf("- HIDS: Registered HID Information characteristic handle: 0x%X\n", hidInformationHandle);

	uuid = REPORT_MAP_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(serviceHandle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			sizeof(reportMap),
			CHAR_PROP_READ,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10,
			CHAR_VALUE_LEN_VARIABLE,
			&(reportMapHandle));
	printf("- HIDS: Registered Report Map characteristic handle: 0x%X\n", reportMapHandle);

	uuid = GYRO_COMMAND_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(serviceHandle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			2,
			CHAR_PROP_WRITE,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_ATTRIBUTE_WRITE,
			10,
			CHAR_VALUE_LEN_VARIABLE,
			&(gyroCommandHandle));
	printf("- HIDS: Registered Gyro command characteristic handle: 0x%X\n", gyroCommandHandle);

	uuid = GYRO_REGISTER_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(serviceHandle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			1,
			CHAR_PROP_READ | CHAR_PROP_NOTIFY,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10,
			CHAR_VALUE_LEN_CONSTANT,
			&(gyroRegisterValHandle));
	printf("- HIDS: Registered Gyro register characteristic handle: 0x%X\n", gyroRegisterValHandle);

	uuid = REPORT_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(serviceHandle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			sizeof(joystickReport),
			CHAR_PROP_READ | CHAR_PROP_WRITE | CHAR_PROP_NOTIFY,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_ATTRIBUTE_WRITE | GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP | GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10,
			CHAR_VALUE_LEN_CONSTANT,
			&(reportJoystickHandle));
	printf("- HIDS: Registered Report characteristic handle: 0x%X\n", reportJoystickHandle);

	// Add char descriptor for each report
	uuid = REPORT_REFERENCE_DESCRIPTOR_UUID;
	hciCmdResult = aci_gatt_add_char_desc(serviceHandle,
			reportJoystickHandle,
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
			&reportJoystickRefDescHandle);
	printf("- HIDS: Registered Report Reference Descriptor handle: 0x%X\n", reportJoystickRefDescHandle);

	if (hciCmdResult != BLE_STATUS_SUCCESS) {
		printf("-- HIDS APPLICATION : Error registering characteristics: 0x%X\n", hciCmdResult);
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
			hidService.serviceHandle,
			hidService.reportJoystickHandle, 0,
			sizeof(hidService.joystickReport),
			(uint8_t*)&hidService.joystickReport
			);
}


void HidService::UpdateReportMapChar()
{
	aci_gatt_update_char_value(serviceHandle, reportMapHandle,	0, sizeof(reportMap), reportMap);
}


void HidService::UpdateHidInformationChar()
{
	aci_gatt_update_char_value(serviceHandle, reportJoystickHandle, 0,	sizeof(joystickReport),	(uint8_t*)&joystickReport);
}


void HidService::UpdateGyroChar()
{
	aci_gatt_update_char_value(hidService.serviceHandle, hidService.gyroRegisterValHandle, 0, 1, &hidService.gyroRegister.val);
}


void HidService::Calibrate()
{
	printf("Starting calibration\r\n");
	calibrateCounter = calibrateCount;
}


void HidService::JoystickNotification(int16_t x, int16_t y, int16_t z)
{
	oldPos = currPos;
	currPos.x = x;
	currPos.y = y;
	currPos.z = z;

	// First time running or calibrating set average movement
	if (averageMovement.x + averageMovement.y + averageMovement.z == 0 || calibrateCounter == calibrateCount) {
		averageMovement.x = (float)x;
		averageMovement.y = (float)y;
		averageMovement.z = (float)z;
	}

	// Check if potentially moving
	if (abs(oldPos.x - x) > compareLimit || abs(oldPos.y - y) > compareLimit || abs(oldPos.z - z) > compareLimit) {
		++countChange[changeArrCounter];
	}
	if (++changeBitCounter == 0) {						// store potential movement counts in 8 blocks of 256 readings
		if (++changeArrCounter > 7) {
			changeArrCounter = 0;
			if (!moving) {								// Store long-term count of non-motion to enable sleep
				++noMovementCount;
			}
		}
		countChange[changeArrCounter] = 0;
		moving &= ~(1 << changeArrCounter);				// Clear the moving bit for the current movement count
	}
	if (countChange[changeArrCounter] > maxChange) {
		moving |= (1 << changeArrCounter);				// Set the moving bit for this value
		noMovementCount = 0;
	}


	if (outputGyro && SysTickVal - lastPrint > 300) {	// Periodically print position to console
		printf("Current: %d, %d, %d; Average: %.1f, %.1f, %.1f\r\n", x, y, z, averageMovement.x, averageMovement.y, averageMovement.z);
		lastPrint = SysTickVal;
	}

	if (moving) {
		gyro.SamplingSpeed(GyroSPI::Sampling::Fast);			// Switch gyro to fast sampling

		// If one of the previous 8 change counts registered movement output data less average offset
		joystickReport.x = std::clamp(x - (int32_t)averageMovement.x, -32768L, 32767L);
		joystickReport.y = std::clamp(y - (int32_t)averageMovement.y, -32768L, 32767L);
		joystickReport.z = std::clamp(z - (int32_t)averageMovement.z, -32768L, 32767L);

		UTIL_SEQ_SetTask(1 << CFG_TASK_JoystickNotification, CFG_SCH_PRIO_0);
	} else {

		if (calibrateCounter) {
			--calibrateCounter;
			if (calibrateCounter == 0) {
				printf("Calibration complete. Offsets: %.1f, %.1f, %.1f\r\n", averageMovement.x, averageMovement.y, averageMovement.z);
				if (!hidService.JoystickNotifications) {					// If not outputting to BLE client start gyro output
					gyro.Configure(GyroSPI::SetConfig::PowerDown);
				}
				config.SaveConfig();
			}
		} else {
			// If inactive for a while go to sleep
			if (noMovementCount > 1) {
				noMovementCount = 0;				// Or will go back to sleep as soon as it wakes up
				bleApp.InactivityTimeout();
				return;
			}
			gyro.SamplingSpeed(GyroSPI::Sampling::Slow);			// Switch gyro to slow sampling
		}


		// If not moving store smoothed movement data to send periodically
		static constexpr float smooth = 0.999f;
		averageMovement.x = (smooth * averageMovement.x) + (1.0f - smooth) * (float)x;
		averageMovement.y = (smooth * averageMovement.y) + (1.0f - smooth) * (float)y;
		averageMovement.z = (smooth * averageMovement.z) + (1.0f - smooth) * (float)z;

		if (SysTickVal - lastSend > 10000) {
			joystickReport.x = 0;
			joystickReport.y = 0;
			joystickReport.z = 0;

			UTIL_SEQ_SetTask(1 << CFG_TASK_JoystickNotification, CFG_SCH_PRIO_0);
			lastSend = SysTickVal;
		}
	}
}


void HidService::ControlPointWrite(uint16_t data) {
	// To inform the device that the host is entering or leaving suspend state
	return;
}


void HidService::Disconnect() {
	JoystickNotifications = false;
	gyro.Configure(GyroSPI::SetConfig::PowerDown);					// Power down Gyroscope
}


bool HidService::EventHandler(hci_event_pckt* event_pckt)
{
	aci_gatt_attribute_modified_event_rp0* attrModifiedEvt;
	aci_gatt_write_permit_req_event_rp0*   writeRequestEvt;
	aci_gatt_read_permit_req_event_rp0*    readRequestEvt;

	bool handled = false;

	evt_blecore_aci* blecore_evt = (evt_blecore_aci*)event_pckt->data;

	switch (blecore_evt->ecode) {

	case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
		attrModifiedEvt = (aci_gatt_attribute_modified_event_rp0*)blecore_evt->data;

		if (attrModifiedEvt->Attr_Handle == (hidService.reportJoystickHandle + DescriptorOffset)) {
			handled = true;

			hidService.JoystickNotifications = (attrModifiedEvt->Attr_Data[0] == 1);
			if (hidService.JoystickNotifications) {
				gyro.Configure(GyroSPI::SetConfig::ContinousOutput);			// Start Gyroscope readings
			} else {
				gyro.Configure(GyroSPI::SetConfig::PowerDown);					// Power down Gyroscope
			}
		}

		if (attrModifiedEvt->Attr_Handle == (hidService.hidControlPointHandle + ValueOffset)) {
			handled = true;
			uint16_t write_data = (attrModifiedEvt->Attr_Data[1] << 8) | attrModifiedEvt->Attr_Data[0];
			hidService.ControlPointWrite(write_data);
		}

		if (attrModifiedEvt->Attr_Handle == (hidService.gyroCommandHandle + ValueOffset)) {
			// Provide mechanism to read and write gyro registers
			// If one byte sent then the value of that register can be read; two bytes to write to a register
			gyroRegister.reg = attrModifiedEvt->Attr_Data[0];					// Store register number
			if (attrModifiedEvt->Attr_Data_Length == 2) {						// writing value
				gyroRegister.val = attrModifiedEvt->Attr_Data[1];
				gyro.WriteCmd(gyroRegister.reg, gyroRegister.val);
			} else if (gyroRegister.reg >= 0x0F && gyroRegister.reg <= 0x38) {		// reading register
				gyroRegister.val = gyro.ReadRegister(gyroRegister.reg);
				UTIL_SEQ_SetTask(1 << CFG_TASK_GyroNotification, CFG_SCH_PRIO_0);
			}
			handled = true;
		}

		break;

	case ACI_GATT_READ_PERMIT_REQ_VSEVT_CODE:
		readRequestEvt = (aci_gatt_read_permit_req_event_rp0*)blecore_evt->data;

		if (readRequestEvt->Attribute_Handle == (hidService.reportMapHandle + ValueOffset)) {
			handled = true;
			aci_gatt_allow_read(readRequestEvt->Connection_Handle);
		}
		if (readRequestEvt->Attribute_Handle == (hidService.reportJoystickHandle + ValueOffset)) {
			aci_gatt_allow_read(readRequestEvt->Connection_Handle);
			handled = true;
			gyro.GyroRead();
			JoystickNotification(gyro.gyroData.x, gyro.gyroData.y, gyro.gyroData.z);
		}
		if (readRequestEvt->Attribute_Handle == (hidService.hidInformationHandle + ValueOffset)) {
			handled = true;
			aci_gatt_allow_read(readRequestEvt->Connection_Handle);
		}
		if (readRequestEvt->Attribute_Handle == (hidService.gyroRegisterValHandle + ValueOffset)) {
			handled = true;
			aci_gatt_allow_read(readRequestEvt->Connection_Handle);
			UTIL_SEQ_SetTask(1 << CFG_TASK_GyroNotification, CFG_SCH_PRIO_0);
		}
		break;

	case ACI_ATT_EXEC_WRITE_RESP_VSEVT_CODE:
		writeRequestEvt = (aci_gatt_write_permit_req_event_rp0*)blecore_evt->data;
		break;

	case ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE:
		writeRequestEvt = (aci_gatt_write_permit_req_event_rp0*)blecore_evt->data;
		if (writeRequestEvt->Attribute_Handle == (hidService.hidControlPointHandle + ValueOffset)) {
			handled = true;
		}

		break;

	default:
		break;
	}


	return handled;
}


uint32_t HidService::SerialiseConfig(uint8_t** buff)
{
	*buff = reinterpret_cast<uint8_t*>(&averageMovement);
	return sizeof(averageMovement);
}


uint32_t HidService::StoreConfig(uint8_t* buff)
{
	if (buff != nullptr) {
		memcpy(&averageMovement, buff, sizeof(averageMovement));
	}

	return sizeof(averageMovement);
}
