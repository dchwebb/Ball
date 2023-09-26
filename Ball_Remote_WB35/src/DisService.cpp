#include <DisService.h>
#include "ble.h"
#include "dbg_trace.h"

DisService disService;

void DisService::Init()
{
	uint16_t uuid = DEVICE_INFORMATION_SERVICE_UUID;
	aci_gatt_add_service(UUID_TYPE_16,
		(Service_UUID_t*)&uuid,
		PRIMARY_SERVICE,
		2+1,
		&(disService.DevInfoSvcHdle));

	printf("- DIS: Registered Device Information Service handle: 0x%X\n", DevInfoSvcHdle);

	// Add Manufacturer Name String Characteristic
	uuid = MANUFACTURER_NAME_UUID;
	aci_gatt_add_char(disService.DevInfoSvcHdle,
		UUID_TYPE_16,
		(Char_UUID_t*) &uuid,
		BLE_CFG_DIS_MANUFACTURER_NAME_STRING_LEN_MAX,
		CHAR_PROP_READ,
		ATTR_PERMISSION_NONE,
		GATT_DONT_NOTIFY_EVENTS,	// gattEvtMask
		10,							// encryKeySize
		CHAR_VALUE_LEN_VARIABLE,
		&(disService.ManufacturerNameStringCharHdle));

	printf("- DIS: Registered Manufacturer Name Characteristic handle: 0x%X \n", ManufacturerNameStringCharHdle);

	aci_gatt_update_char_value(disService.DevInfoSvcHdle, disService.ManufacturerNameStringCharHdle, 0, 8, DIS_Name);
}
