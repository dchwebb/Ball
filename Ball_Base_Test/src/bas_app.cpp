#include "ble.h"
#include "stm32_seq.h"
#include "bas_app.h"
#include <algorithm>

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
			&(serviceHandle));
	APP_DBG_MSG("- BAS: Registered BAS Service handle: 0x%X\n", serviceHandle);

	uuid = BATTERY_LEVEL_CHAR_UUID;
	hciCmdResult = aci_gatt_add_char(serviceHandle,
			UUID_TYPE_16,
			(Char_UUID_t*)&uuid,
			4,										// Char value length
			CHAR_PROP_READ | CHAR_PROP_NOTIFY,
			ATTR_PERMISSION_NONE,
			GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP,
			10, 									// encryKeySize
			CHAR_VALUE_LEN_CONSTANT,
			&(batteryLevelHandle));
	APP_DBG_MSG("- BAS: Registered Battery Level characteristic handle: 0x%X\n", batteryLevelHandle);

	if (hciCmdResult != BLE_STATUS_SUCCESS) {
		APP_DBG_MSG("- BAS: Error registering characteristics: 0x%X\n", hciCmdResult);
	}
	AppInit();
}


void BasService::AppInit()
{
	batteryNotifications = false;
	GetBatteryLevel();

	// Use the sequencer to update the report char so that to avoid write conflicts
	UTIL_SEQ_RegTask(1 << CFG_TASK_BatteryNotification, 0, UpdateBatteryChar);
	UpdateBatteryChar();
}


void BasService::UpdateBatteryChar()
{
	// Will trigger a notification to be sent if client is subscribed (called from sequencer)
	aci_gatt_update_char_value(
			basService.serviceHandle,
			basService.batteryLevelHandle,
			0,
			2,													// Size of battery report
			(uint8_t*)&basService.Level);
}


float BasService::GetBatteryLevel()
{
	// Get Battery level as voltage
	// voltage divider scales 5V > 2.9V (charging); 4.3V > 2.46V (Fully charged); 3.2V > 1.85V (battery lowest usable level)
	// ADC 4096 / 2.8V = 1462; calculated to scale by 1.73 (values corrected by measurement)

	float voltage = (static_cast<float>(2000) / 1381.0f) * 1.73f;
	Level = static_cast<uint8_t>(std::clamp((voltage - 3.2f) * 91.0f, 0.0f, 100.0f));		// convert voltage to 0 - 100 range for 1.8V - 4.3V
	return voltage;
}


void BasService::TimedRead()
{
	// Called in main loop - checks battery level every 5 seconds and updates characteristic if required
	if (lastRead + 5000 < SysTickVal) {
		lastRead = SysTickVal;

		uint8_t oldLevel = Level;
		GetBatteryLevel();

		if (Level != oldLevel || lastSent + 10000 < SysTickVal) {				// FIXME - force a resend of battery every 10 seconds (make longer in production)
			UTIL_SEQ_SetTask(1 << CFG_TASK_BatteryNotification, CFG_SCH_PRIO_0);
			lastSent = SysTickVal;
		}

		//ADC1->CR |= ADC_CR_ADSTART;			// Trigger next ADC read
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
		if (attribute_modified->Attr_Handle == (batteryLevelHandle + 2)) {		// 2 = Offset of descriptor from characteristic handle
			handled = true;

			if (attribute_modified->Attr_Data[0] == 1) {
				batteryNotifications = true;
				//UpdateBatteryChar();
			} else {
				batteryNotifications = false;
			}
		}
		break;
	}

	case ACI_GATT_READ_PERMIT_REQ_VSEVT_CODE:
	{
		auto read_req = (aci_gatt_read_permit_req_event_rp0*)blecore_evt->data;
		if (read_req->Attribute_Handle == (batteryLevelHandle + 1)) {		// 1 = Offset of value from characteristic handle
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


