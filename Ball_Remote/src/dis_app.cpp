#include "ble.h"
#include "dis_app.h"

DisService disService;

void DisService::Init()
{
	uint16_t uuid = DEVICE_INFORMATION_SERVICE_UUID;
	tBleStatus hciCmdResult = aci_gatt_add_service(UUID_TYPE_16,
			(Service_UUID_t*)&uuid,
			PRIMARY_SERVICE,
			2+1,
			&(disService.DevInfoSvcHdle));

	if (hciCmdResult == BLE_STATUS_SUCCESS) {
		BLE_DBG_DIS_MSG ("- DIS: Registered Device Information Service handle: 0x%X\n", disService.DevInfoSvcHdle);
	} else {
		BLE_DBG_DIS_MSG ("FAILED to add Device Information Service (DIS), Error: %02X\n", hciCmdResult);
	}

	// Add Manufacturer Name String Characteristic
	uuid = MANUFACTURER_NAME_UUID;
	hciCmdResult = aci_gatt_add_char(disService.DevInfoSvcHdle,
			UUID_TYPE_16,
			(Char_UUID_t*) &uuid,
			BLE_CFG_DIS_MANUFACTURER_NAME_STRING_LEN_MAX,
			CHAR_PROP_READ,
			ATTR_PERMISSION_NONE,
			GATT_DONT_NOTIFY_EVENTS,	// gattEvtMask
			10,							// encryKeySize
			CHAR_VALUE_LEN_VARIABLE,
			&(disService.ManufacturerNameStringCharHdle));

	if (hciCmdResult == BLE_STATUS_SUCCESS) {
		BLE_DBG_DIS_MSG ("- DIS: Registered Manufacturer Name Characteristic handle: 0x%X \n", disService.ManufacturerNameStringCharHdle);
	} else {
		BLE_DBG_DIS_MSG ("FAILED to add Manufacturer Name Characteristic, Error: %02X !!\n", hciCmdResult);
	}

	AppInit();
}

void DisService::AppInit()
{
	aci_gatt_update_char_value(disService.DevInfoSvcHdle, disService.ManufacturerNameStringCharHdle, 0, 8, DIS_Name);
}
