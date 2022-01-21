#include "main.h"
#include "dbg_trace.h"
#include "ble.h"
#include "app_ble.h"
#include "stm32_seq.h"
#include "shci.h"
#include "stm32_lpm.h"
#include "dis_app.h"
#include "hids_app.h"
#include "bas_app.h"

BleApp bleApp;


PLACE_IN_SECTION("MB_MEM1") ALIGN(4) static TL_CmdPacket_t BleCmdBuffer;

// These are the two tags used to manage a power failure during OTA
// The MagicKeywordAdress shall be mapped @0x140 from start of the binary image
// The MagicKeywordvalue is checked in the ble_ota application
PLACE_IN_SECTION("TAG_OTA_END") const uint32_t MagicKeywordValue = 0x94448A29;
PLACE_IN_SECTION("TAG_OTA_START") const uint32_t MagicKeywordAddress = (uint32_t)&MagicKeywordValue;


void BleApp::Init()
{
	SHCI_C2_Ble_Init_Cmd_Packet_t ble_init_cmd_packet =	{
			{{0,0,0}},                          // Header unused
			{0,                                 // pBleBufferAddress not used
			0,                         			// BleBufferSize not used
			CFG_BLE_NUM_GATT_ATTRIBUTES,
			CFG_BLE_NUM_GATT_SERVICES,
			CFG_BLE_ATT_VALUE_ARRAY_SIZE,
			CFG_BLE_NUM_LINK,
			CFG_BLE_DATA_LENGTH_EXTENSION,
			CFG_BLE_PREPARE_WRITE_LIST_SIZE,
			CFG_BLE_MBLOCK_COUNT,
			CFG_BLE_MAX_ATT_MTU,
			CFG_BLE_SLAVE_SCA,
			CFG_BLE_MASTER_SCA,
			CFG_BLE_LSE_SOURCE,
			CFG_BLE_MAX_CONN_EVENT_LENGTH,
			CFG_BLE_HSE_STARTUP_TIME,
			CFG_BLE_VITERBI_MODE,
			CFG_BLE_OPTIONS,
			0,
			CFG_BLE_MAX_COC_INITIATOR_NBR,
			CFG_BLE_MIN_TX_POWER,
			CFG_BLE_MAX_TX_POWER}
	};

	TransportLayerInit();												// Initialize Ble Transport Layer
	//UTIL_LPM_SetOffMode(1, UTIL_LPM_DISABLE);							// Do not allow standby in the application

	// Register the hci transport layer to handle BLE User Asynchronous Events
	UTIL_SEQ_RegTask(1 << CFG_TASK_HCI_ASYNCH_EVT_ID, UTIL_SEQ_RFU, hci_user_evt_proc);

	SHCI_CmdStatus_t result = SHCI_C2_BLE_Init(&ble_init_cmd_packet);
	if (result != SHCI_Success) {		// Starts the BLE Stack on CPU2
		Error_Handler();
	}

	HciGapGattInit();													// Initialization of HCI & GATT & GAP layer
	SVCCTL_Init();														// Initialization of the BLE Services

	UTIL_SEQ_RegTask(1 << CFG_TASK_SwitchLPAdvertising, UTIL_SEQ_RFU, SwitchLPAdvertising);
	UTIL_SEQ_RegTask(1 << CFG_TASK_CancelAdvertising, UTIL_SEQ_RFU, CancelAdvertising);

	// Allows masked radio activity to trigger event
	// 0x01: Idle, 0x02: Advertising, 0x04: Connection event slave, 0x08: Scanning, 0x10: Connection request, 0x20: Connection event master, 0x40: TX test mode, 0x80: RX test mode
	aci_hal_set_radio_activity_mask(0x0006);

	devInfoService.AppInit();											// Initialize DIS Application
	BAS_App_Init();														// Initialize Battery Service
	hidService.AppInit();												// Initialise HID Service

	// Create timer to switch to Low Power advertising
	HW_TS_Create(CFG_TIM_PROC_ID_ISR, &(lowPowerAdvTimerId), hw_ts_SingleShot, QueueLPAdvertising);

	EnableAdvertising(ConnStatus::FastAdv);								// Start advertising for client connections

}


SVCCTL_UserEvtFlowStatus_t SVCCTL_App_Notification(void* pckt)
{
	bleApp.ServiceControlCallback(pckt);
	return SVCCTL_UserEvtFlowEnable;
}

void BleApp::ServiceControlCallback(void* pckt)
{
	hci_le_phy_update_complete_event_rp0 *evt_le_phy_update_complete;
	uint8_t TX_PHY, RX_PHY;
	tBleStatus ret = BLE_STATUS_INVALID_PARAMS;

	hci_event_pckt* event_pckt = (hci_event_pckt*) ((hci_uart_pckt*) pckt)->data;

	// PAIRING
	aci_gap_numeric_comparison_value_event_rp0 *evt_numeric_value;
	aci_gap_pairing_complete_event_rp0 *pairing_complete;
	uint32_t numeric_value;

	switch (event_pckt->evt) {
	case HCI_DISCONNECTION_COMPLETE_EVT_CODE:
	{
		//hci_disconnection_complete_event_rp0* disconnection_complete_event = (hci_disconnection_complete_event_rp0*) event_pckt->data;

		connectionHandle = 0xFFFF;
		connectionStatus = ConnStatus::Idle;
		hidService.Disconnect();

		APP_DBG_MSG("\r\n\r** DISCONNECTION EVENT WITH CLIENT \n");

		EnableAdvertising(ConnStatus::FastAdv);			// restart advertising

		HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, GPIO_PIN_RESET);
	}

	break;

	case HCI_LE_META_EVT_CODE:
	{
		evt_le_meta_event* meta_evt = (evt_le_meta_event*) event_pckt->data;

		switch (meta_evt->subevent)	{
			case HCI_LE_CONNECTION_UPDATE_COMPLETE_SUBEVT_CODE:
				APP_DBG_MSG("\r\n\r** CONNECTION UPDATE EVENT WITH CLIENT \n");
				break;

			case HCI_LE_PHY_UPDATE_COMPLETE_SUBEVT_CODE:
				APP_DBG_MSG("EVT_UPDATE_PHY_COMPLETE \n");
				evt_le_phy_update_complete = (hci_le_phy_update_complete_event_rp0*)meta_evt->data;
				if (evt_le_phy_update_complete->Status == 0) {
					APP_DBG_MSG("EVT_UPDATE_PHY_COMPLETE, status ok \n");
				} else {
					APP_DBG_MSG("EVT_UPDATE_PHY_COMPLETE, status nok \n");
				}

				ret = hci_le_read_phy(connectionHandle, &TX_PHY, &RX_PHY);
				if (ret == BLE_STATUS_SUCCESS) {
					APP_DBG_MSG("Read_PHY success \n");

					if ((TX_PHY == TX_2M) && (RX_PHY == RX_2M))	{
						APP_DBG_MSG("PHY Param  TX= %d, RX= %d \n", TX_PHY, RX_PHY);
					} else {
						APP_DBG_MSG("PHY Param  TX= %d, RX= %d \n", TX_PHY, RX_PHY);
					}
				} else {
					APP_DBG_MSG("Read conf unsuccessful \n");
				}
				break;

			case HCI_LE_CONNECTION_COMPLETE_SUBEVT_CODE:
			{
				hci_le_connection_complete_event_rp0* connCompleteEvent = (hci_le_connection_complete_event_rp0*) meta_evt->data;

				HW_TS_Stop(lowPowerAdvTimerId);					// connected: no need anymore to schedule the LP ADV

				APP_DBG_MSG("HCI_LE_CONNECTION_COMPLETE_SUBEVT_CODE for connection handle 0x%x\n", connCompleteEvent->Connection_Handle);
				connectionStatus = ConnStatus::Connected;
				connectionHandle = connCompleteEvent->Connection_Handle;

				HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, GPIO_PIN_SET);
				HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_RESET);		// Turn off advertising LED
			}
			break;

			default:
				break;
		}
	}
	break;

	case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
	{
		evt_blecore_aci* blecore_evt = (evt_blecore_aci*) event_pckt->data;

		switch (blecore_evt->ecode)	{
			case ACI_L2CAP_CONNECTION_UPDATE_RESP_VSEVT_CODE:
				break;

			case ACI_GAP_PROC_COMPLETE_VSEVT_CODE:
				APP_DBG_MSG("\r\n\r** ACI_GAP_PROC_COMPLETE_VSEVT_CODE \n");
				break;

			case ACI_HAL_END_OF_RADIO_ACTIVITY_VSEVT_CODE:
				break;

			case (ACI_GAP_KEYPRESS_NOTIFICATION_VSEVT_CODE):
				APP_DBG_MSG("\r\n\r** ACI_GAP_KEYPRESS_NOTIFICATION_VSEVT_CODE \n");
				break;

			case ACI_GAP_PASS_KEY_REQ_VSEVT_CODE:
				aci_gap_pass_key_resp(connectionHandle, CFG_FIXED_PIN);
				break;

			case ACI_GAP_NUMERIC_COMPARISON_VALUE_VSEVT_CODE:
				evt_numeric_value = (aci_gap_numeric_comparison_value_event_rp0*)blecore_evt->data;
				numeric_value = evt_numeric_value->Numeric_Value;
				APP_DBG_MSG("numeric_value = %ld\n", numeric_value);
				aci_gap_numeric_comparison_value_confirm_yesno(connectionHandle, YES);
				break;

			case ACI_GAP_PAIRING_COMPLETE_VSEVT_CODE:
				pairing_complete = (aci_gap_pairing_complete_event_rp0*)blecore_evt->data;
				APP_DBG_MSG("BLE_CTRL_App_Notification: ACI_GAP_PAIRING_COMPLETE_VSEVT_CODE, pairing_complete->Status = %d\n", pairing_complete->Status);
				break;
		}
		break;
	}

		default:
			break;
	}


}


void BleApp::TransportLayerInit()
{
	HCI_TL_HciInitConf_t Hci_Tl_Init_Conf;

	Hci_Tl_Init_Conf.p_cmdbuffer = (uint8_t*)&BleCmdBuffer;
	Hci_Tl_Init_Conf.StatusNotCallBack = StatusNot;
	hci_init(UserEvtRx, (void*) &Hci_Tl_Init_Conf);
}


void BleApp::HciGapGattInit()
{
	uint16_t gap_service_handle, gap_dev_name_char_handle, gap_appearance_char_handle;

	uint16_t appearance[1] = { BLE_CFG_GAP_APPEARANCE };

	hci_reset();									// HCI Reset to synchronise BLE Stack

	// Write the BD Address
	aci_hal_write_config_data(CONFIG_DATA_PUBADDR_OFFSET, bdAddrSize, GetBdAddress());

	// Write Identity root key used to derive LTK and CSRK
	aci_hal_write_config_data(CONFIG_DATA_IR_OFFSET, CONFIG_DATA_IR_LEN, IdentityRootKey);

	// Write Encryption root key used to derive LTK and CSRK
	aci_hal_write_config_data(CONFIG_DATA_ER_OFFSET, CONFIG_DATA_ER_LEN, EncryptionRootKey);

	aci_hal_set_tx_power_level(1, CFG_TX_POWER);	// Set TX Power to 0dBm.
	aci_gatt_init();								// Initialize GATT interface

	// Initialize GAP interface
	const char* name = CFG_GAP_DEVICE_NAME;
	aci_gap_init(GAP_PERIPHERAL_ROLE, 0, CFG_GAP_DEVICE_NAME_LENGTH, &gap_service_handle, &gap_dev_name_char_handle, &gap_appearance_char_handle);

	if (aci_gatt_update_char_value(gap_service_handle, gap_dev_name_char_handle, 0, strlen(name), (uint8_t*)name)) {
		BLE_DBG_SVCCTL_MSG("Device Name aci_gatt_update_char_value failed.\n");
	}

	if (aci_gatt_update_char_value(gap_service_handle, gap_appearance_char_handle, 0, 2, (uint8_t*)&appearance)) {
		BLE_DBG_SVCCTL_MSG("Appearance aci_gatt_update_char_value failed.\n");
	}

	// Initialize Default PHY
	hci_le_set_default_phy(ALL_PHYS_PREFERENCE, TX_2M_PREFERRED, RX_2M_PREFERRED);

	// Initialize IO capability
	aci_gap_set_io_capability(Security.ioCapability);

	// Initialize authentication
	aci_gap_set_authentication_requirement(Security.bondingMode,
			Security.mitmMode,
			CFG_SC_SUPPORT,
			CFG_KEYPRESS_NOTIFICATION_SUPPORT,
			Security.encryptionKeySizeMin,
			Security.encryptionKeySizeMax,
			Security.Use_Fixed_Pin,
			Security.fixedPin,
			CFG_BLE_ADDRESS_TYPE
	);

	// Initialize whitelist
	if (Security.bondingMode) {
		aci_gap_configure_whitelist();
	}
}


void BleApp::EnableAdvertising(ConnStatus newStatus)
{
	tBleStatus ret = BLE_STATUS_INVALID_PARAMS;
	uint16_t MinInterval = (newStatus == ConnStatus::FastAdv) ? FastAdvIntervalMin : LPAdvIntervalMin;
	uint16_t MaxInterval = (newStatus == ConnStatus::FastAdv) ? FastAdvIntervalMax : LPAdvIntervalMax;

	// Stop the timer, it will be restarted for a new shot; It does not hurt if the timer was not running
	HW_TS_Stop(lowPowerAdvTimerId);

	// If switching to LP advertising first disable fast advertising
	if ((newStatus == ConnStatus::LPAdv) && ((connectionStatus == ConnStatus::FastAdv) || (connectionStatus == ConnStatus::LPAdv))) {
		ret = aci_gap_set_non_discoverable();
		if (ret == BLE_STATUS_SUCCESS) {
			APP_DBG_MSG("Stopped Advertising \n");
		} else {
			APP_DBG_MSG("Stop Advertising Failed , result: %d \n", ret);
		}
	}
	connectionStatus = newStatus;

	ret = aci_gap_set_discoverable(ADV_TYPE, MinInterval, MaxInterval, CFG_BLE_ADDRESS_TYPE, ADV_FILTER, 0, 0, 0, 0, 0, 0);

	// Update Advertising data
	ret = aci_gap_update_adv_data(sizeof(ad_data), ad_data);

	if (ret == BLE_STATUS_SUCCESS) {
		HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_SET);

		if (newStatus == ConnStatus::FastAdv)	{
			APP_DBG_MSG("Start Fast Advertising \n");
			// Start Timer to STOP ADV - TIMEOUT
			HW_TS_Start(lowPowerAdvTimerId, FastAdvTimeout);
		} else {
			APP_DBG_MSG("Start Low Power Advertising \n");
		}
	} else {
		APP_DBG_MSG("Start Advertising Failed , result: %d \n", ret);
	}

}


uint8_t* BleApp::GetBdAddress()
{
	uint32_t udn = LL_FLASH_GetUDN();
	uint32_t company_id = LL_FLASH_GetSTCompanyID();
	uint32_t device_id = LL_FLASH_GetDeviceID();

	// Public Address with the ST company ID
	// bit[47:24] : 24 bits (OUI) equal to the company ID
	// bit[23:16] : Device ID.
	// bit[15:0]  : The last 16 bits from the UDN
	// Note: In order to use the Public Address in a final product, a dedicated 24 bit company ID (OUI) shall be bought.
	bd_addr_udn[0] = (uint8_t)(udn & 0x000000FF);
	bd_addr_udn[1] = (uint8_t)((udn & 0x0000FF00) >> 8);
	bd_addr_udn[2] = (uint8_t)device_id;
	bd_addr_udn[3] = (uint8_t)(company_id & 0x000000FF);
	bd_addr_udn[4] = (uint8_t)((company_id & 0x0000FF00) >> 8);
	bd_addr_udn[5] = (uint8_t)((company_id & 0x00FF0000) >> 16);

	return bd_addr_udn;
}


void BleApp::QueueLPAdvertising()
{
	// Queues transition from fast to lop wer advertising
	UTIL_SEQ_SetTask(1 << CFG_TASK_SwitchLPAdvertising, CFG_SCH_PRIO_0);
}


void BleApp::SwitchLPAdvertising()
{
	bleApp.EnableAdvertising(ConnStatus::LPAdv);
}


void BleApp::CancelAdvertising()
{
	if (bleApp.connectionStatus != ConnStatus::Connected) {
		HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_RESET);
		tBleStatus result = aci_gap_set_non_discoverable();

		bleApp.connectionStatus = ConnStatus::Idle;
		if (result == BLE_STATUS_SUCCESS) {
			APP_DBG_MSG("** STOP ADVERTISING **  \r\n\r");
		} else {
			APP_DBG_MSG("** STOP ADVERTISING **  Failed \r\n\r");
		}
	}
}


void hci_notify_asynch_evt(void* pdata)
{
	UTIL_SEQ_SetTask(1 << CFG_TASK_HCI_ASYNCH_EVT_ID, CFG_SCH_PRIO_0);
}


void hci_cmd_resp_release(uint32_t flag)
{
	UTIL_SEQ_SetEvt(1 << CFG_IDLEEVT_HCI_CMD_EVT_RSP_ID);
}


void hci_cmd_resp_wait(uint32_t timeout)
{
	UTIL_SEQ_WaitEvt(1 << CFG_IDLEEVT_HCI_CMD_EVT_RSP_ID);
}

void BleApp::UserEvtRx(void * pPayload)
{
	tHCI_UserEvtRxParam* pParam = (tHCI_UserEvtRxParam*)pPayload;
	SVCCTL_UserEvtFlowStatus_t svctl_return_status = SVCCTL_UserEvtRx((void*)&(pParam->pckt->evtserial));

	if (svctl_return_status != SVCCTL_UserEvtFlowDisable) {
		pParam->status = HCI_TL_UserEventFlow_Enable;
	} else {
		pParam->status = HCI_TL_UserEventFlow_Disable;
	}
}


void BleApp::StatusNot(HCI_TL_CmdStatus_t status)
{
	uint32_t task_id_list;
	switch (status)	{
	case HCI_TL_CmdBusy:
		// All tasks that may send an aci/hci commands shall be listed here:  This is to prevent a new command is sent while one is already pending
		task_id_list = (1 << CFG_LAST_TASK_ID_WITH_HCICMD) - 1;
		UTIL_SEQ_PauseTask(task_id_list);
		break;

	case HCI_TL_CmdAvailable:
		task_id_list = (1 << CFG_LAST_TASK_ID_WITH_HCICMD) - 1;
		UTIL_SEQ_ResumeTask(task_id_list);
		break;

	default:
		break;
	}
}


void SVCCTL_ResumeUserEventFlow(void)
{
	hci_resume_flow();
}

