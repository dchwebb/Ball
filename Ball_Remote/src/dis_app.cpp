#include "app_common.h"
#include "ble.h"
#include "dis_app.h"
#include "common_blesvc.h"

DevInfoService devInfoService;

// called from external C prog svc_ctl.c
void DIS_Init()
{
	devInfoService.Init();
}

void DevInfoService::AppInit()
{
	aci_gatt_update_char_value(devInfoService.DevInfoSvcHdle, devInfoService.ManufacturerNameStringCharHdle, 0, 8, DIS_Name);
}


void DevInfoService::Init()
{
	uint16_t uuid = DEVICE_INFORMATION_SERVICE_UUID;
	tBleStatus hciCmdResult = aci_gatt_add_service(UUID_TYPE_16,
			(Service_UUID_t*)&uuid,
			PRIMARY_SERVICE,
			2+1,
			&(devInfoService.DevInfoSvcHdle));

	if (hciCmdResult == BLE_STATUS_SUCCESS) {
		BLE_DBG_DIS_MSG ("- DIS: Registered Device Information Service handle: 0x%X\n", devInfoService.DevInfoSvcHdle);
	} else {
		BLE_DBG_DIS_MSG ("FAILED to add Device Information Service (DIS), Error: %02X\n", hciCmdResult);
	}

	// Add Manufacturer Name String Characteristic
	uuid = MANUFACTURER_NAME_UUID;
	hciCmdResult = aci_gatt_add_char(devInfoService.DevInfoSvcHdle,
			UUID_TYPE_16,
			(Char_UUID_t*) &uuid,
			BLE_CFG_DIS_MANUFACTURER_NAME_STRING_LEN_MAX,
			CHAR_PROP_READ,
			ATTR_PERMISSION_NONE,
			GATT_DONT_NOTIFY_EVENTS,	// gattEvtMask
			10,							// encryKeySize
			CHAR_VALUE_LEN_VARIABLE,
			&(devInfoService.ManufacturerNameStringCharHdle));

	if (hciCmdResult == BLE_STATUS_SUCCESS) {
		BLE_DBG_DIS_MSG ("- DIS: Registered Manufacturer Name Characteristic handle: 0x%X \n", devInfoService.ManufacturerNameStringCharHdle);
	} else {
		BLE_DBG_DIS_MSG ("FAILED to add Manufacturer Name Characteristic, Error: %02X !!\n", hciCmdResult);
	}

}

