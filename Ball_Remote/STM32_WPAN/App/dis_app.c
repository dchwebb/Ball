#include "app_common.h"
#include "ble.h"
#include "dis_app.h"


#if ((BLE_CFG_DIS_SYSTEM_ID != 0) || (CFG_MENU_DEVICE_INFORMATION != 0))
static const uint8_t system_id[BLE_CFG_DIS_SYSTEM_ID_LEN_MAX] =
{
		(uint8_t)((DISAPP_MANUFACTURER_ID & 0xFF0000) >> 16),
		(uint8_t)((DISAPP_MANUFACTURER_ID & 0x00FF00) >> 8),
		(uint8_t)(DISAPP_MANUFACTURER_ID & 0x0000FF),
		0xFE,
		0xFF,
		(uint8_t)((DISAPP_OUI & 0xFF0000) >> 16),
		(uint8_t)((DISAPP_OUI & 0x00FF00) >> 8),
		(uint8_t)(DISAPP_OUI & 0x0000FF)
};
#endif

#if ((BLE_CFG_DIS_IEEE_CERTIFICATION != 0) || (CFG_MENU_DEVICE_INFORMATION != 0))
static const uint8_t ieee_id[BLE_CFG_DIS_IEEE_CERTIFICATION_LEN_MAX] =
{
		0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA,
		0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA,
		0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA,
		0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA,
};
#endif
#if ((BLE_CFG_DIS_PNP_ID != 0) || (CFG_MENU_DEVICE_INFORMATION != 0))
static const uint8_t pnp_id[BLE_CFG_DIS_PNP_ID_LEN_MAX] =
{
		0x1,
		0xAD, 0xDE,
		0xDE, 0xDA,
		0x01, 0x00
};
#endif

const uint8_t DIS_Name[] = "MOUNTJOY";

DIS_Data_t DISNameStruct = {&DIS_Name, 8};

void DISAPP_Init(void)
{
	DIS_UpdateChar(MANUFACTURER_NAME_UUID, &DISNameStruct);

	/* USER CODE BEGIN DISAPP_Init */

	/* USER CODE END DISAPP_Init */
}

/* USER CODE BEGIN FD */

/* USER CODE END FD */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
