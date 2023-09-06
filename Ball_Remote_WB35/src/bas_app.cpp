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
	basService.BatteryNotifications = false;				// Initialise notification info

	GetBatteryLevel();

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


float BasService::GetBatteryLevel()
{
	// Get Battery level as voltage
	// voltage divider scales 5V > 2.9V (charging); 4.3V > 2.46V (Fully charged); 3.2V > 1.85V (battery lowest usable level)
	// ADC 4096 / 2.8V = 1462; calculated to scale by 1.73 (values corrected by measurement)
	ADC1->ISR &= ~ADC_ISR_EOC;
	ADC1->CR |= ADC_CR_ADSTART;
	while ((ADC1->ISR & ADC_ISR_EOC) != ADC_ISR_EOC) {}
	float voltage = (static_cast<float>(ADC1->DR) / 1390.0f) * 1.73f;

	Level = static_cast<uint8_t>(std::clamp((voltage - 1.8f) * 40.0f, 0.0f, 100.0f));		// convert voltage to 0 - 100 range for 1.8V - 4.3
	return voltage;
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


