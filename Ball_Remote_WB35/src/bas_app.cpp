#include "ble.h"
#include "stm32_seq.h"
#include "bas_app.h"


BasService basService;

void BasService::Init()
{
	uint16_t uuid;
	tBleStatus hciCmdResult = BLE_STATUS_SUCCESS;

	uuid = BATTERY_SERVICE_UUID;
	hciCmdResult = aci_gatt_add_service(UUID_TYPE_16,
			(Service_UUID_t*)&uuid,
			PRIMARY_SERVICE,
			4,										// Max_Attribute_Records
			&(basService.ServiceHandle));
	APP_DBG_MSG("- BAS: Registered BAS Service handle: 0x%X\n", basService.ServiceHandle);

	uuid = BATTERY_LEVEL_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(basService.ServiceHandle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			4,										// Char value length
			CHAR_PROP_READ | CHAR_PROP_NOTIFY,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 									// encryKeySize
			CHAR_VALUE_LEN_CONSTANT,
			&(basService.BatteryLevelHandle));
	APP_DBG_MSG("- BAS: Registered Battery Level characteristic handle: 0x%X\n", basService.BatteryLevelHandle);

	if (hciCmdResult != BLE_STATUS_SUCCESS) {
		APP_DBG_MSG("- BAS: Error registering characteristics: 0x%X\n", hciCmdResult);
	}
	AppInit();
}


void BasService::AppInit()
{
	UTIL_SEQ_RegTask(1 << CFG_TASK_BAS_LEVEL, UTIL_SEQ_RFU, SendNotification);		// FIXME - not currently using
	basService.BatteryNotifications = false;			// Initialise notification info
	basService.Level = 0x42;								// Initialize Level
	UpdateChar();
}


// Update Battery level characteristic value
tBleStatus BasService::UpdateChar()
{
	return aci_gatt_update_char_value(basService.ServiceHandle, basService.BatteryLevelHandle, 0, 2, (uint8_t*)&basService.Level);
}


void BasService::SendNotification()
{
	basService.UpdateChar();
}


void BasService::SetLevel(uint8_t level)
{
	basService.Level = level;
	if (basService.BatteryNotifications) {
		UpdateChar();
	} else {
		APP_DBG_MSG("- BAS: Notifications disabled (set to %d)\r\n", level);
	}
}


bool BasService::EventHandler(hci_event_pckt* event_pckt)
{
	bool handled = false;

	evt_blecore_aci* blecore_evt = (evt_blecore_aci*)event_pckt->data;

	switch (blecore_evt->ecode) {
	case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
	{
		auto attribute_modified = (aci_gatt_attribute_modified_event_rp0*)blecore_evt->data;
		if (attribute_modified->Attr_Handle == (basService.BatteryLevelHandle + 2)) {		// 2 = Offset of descriptor from characteristic handle
			handled = true;

			if (attribute_modified->Attr_Data[0] == 1) {
				basService.BatteryNotifications = true;
				SendNotification();
			} else {
				basService.BatteryNotifications = false;
			}
		}
		break;
	}

	case ACI_GATT_READ_PERMIT_REQ_VSEVT_CODE:
	{
		auto read_req = (aci_gatt_read_permit_req_event_rp0*)blecore_evt->data;
		if (read_req->Attribute_Handle == (basService.BatteryLevelHandle + 1)) {		// 1 = Offset of value from characteristic handle
			handled = true;
			aci_gatt_allow_read(read_req->Connection_Handle);
		}
		break;
	}

	default:
		break;
	}

	return handled;
}

