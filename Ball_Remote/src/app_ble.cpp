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

// security parameters structure
typedef struct _tSecurityParams {
	uint8_t ioCapability = CFG_IO_CAPABILITY;					// IO capability of the device
	uint8_t mitm_mode = CFG_MITM_PROTECTION;					// Man In the Middle protection required?
	uint8_t bonding_mode = CFG_BONDING_MODE;					// bonding mode of the device
	uint8_t Use_Fixed_Pin = CFG_USED_FIXED_PIN;					// 0 = use fixed pin; 1 = request for passkey
	uint8_t encryptionKeySizeMin = CFG_ENCRYPTION_KEY_SIZE_MIN;	// minimum encryption key size requirement
	uint8_t encryptionKeySizeMax = CFG_ENCRYPTION_KEY_SIZE_MAX;	// maximum encryption key size requirement
	uint32_t Fixed_Pin = CFG_FIXED_PIN;							// Fixed pin for pairing process if Use_Fixed_Pin = 1

	//	 0x00 : no security required
	//	 0x01 : host should initiate security by sending the slave security  request command
	//	 0x02 : host need not send the slave security request but it has to wait for pairing to complete before doing any other	processing
	uint8_t initiateSecurity;
} tSecurityParams;


typedef struct {
	uint16_t connectionHandle;			// connection handle of the current active connection When not in connection, the handle is set to 0xFFFF
	tSecurityParams bleSecurityParam;	// security requirements of the host
	APP_BLE_ConnStatus_t connectionStatus;
	uint8_t Advertising_mgr_timer_Id;	// ID of the Advertising Timeout
} BleAppContext_t;


#define FAST_ADV_TIMEOUT               (30 * 1000 * 1000 / CFG_TS_TICK_VAL) 	// 30s
#define INITIAL_ADV_TIMEOUT            (60 * 1000 * 1000 / CFG_TS_TICK_VAL) 	// 60s
#define BD_ADDR_SIZE_LOCAL    6

PLACE_IN_SECTION("MB_MEM1") ALIGN(4) static TL_CmdPacket_t BleCmdBuffer;

static uint8_t bd_addr_udn[BD_ADDR_SIZE_LOCAL];
static const uint8_t BLE_CFG_IR_VALUE[16] = CFG_BLE_IRK;	// Identity root key used to derive LTK and CSRK
static const uint8_t BLE_CFG_ER_VALUE[16] = CFG_BLE_ERK;	// Encryption root key used to derive LTK and CSRK


// These are the two tags used to manage a power failure during OTA
// The MagicKeywordAdress shall be mapped @0x140 from start of the binary image
// The MagicKeywordvalue is checked in the ble_ota application
PLACE_IN_SECTION("TAG_OTA_END") const uint32_t MagicKeywordValue = 0x94448A29;
PLACE_IN_SECTION("TAG_OTA_START") const uint32_t MagicKeywordAddress = (uint32_t)&MagicKeywordValue;

PLACE_IN_SECTION("BLE_APP_CONTEXT") static BleAppContext_t BleAppContext;
PLACE_IN_SECTION("BLE_APP_CONTEXT") static uint16_t AdvIntervalMin, AdvIntervalMax;


// Advertising Data: Length | Type | Data
uint8_t ad_data[25] = {
	2, AD_TYPE_TX_POWER_LEVEL, 0, 							// Transmission Power -0.15dBm
	9, AD_TYPE_COMPLETE_LOCAL_NAME, 'M', 'O', 'U', 'N', 'T', 'J', 'O', 'Y', 	// Complete name
	3, AD_TYPE_16_BIT_SERV_UUID_CMPLT_LIST, 0x12, 0x18,		// 0x1812 = HID
	7, AD_TYPE_MANUFACTURER_SPECIFIC_DATA, 0x01, 0x83, 0xDE, 0xAD, 0xBE, 0xEF
};


// Private function prototypes -----------------------------------------------
static void BLE_UserEvtRx(void* pPayload);
static void BLE_StatusNot(HCI_TL_CmdStatus_t status);
static void Ble_Tl_Init();
static void Ble_Hci_Gap_Gatt_Init();
static uint8_t* BleGetBdAddress();
static void Adv_Request(APP_BLE_ConnStatus_t New_Status);
static void Adv_Mgr();
static void Adv_Update();
static void Adv_Cancel();


void APP_BLE_Init(void)
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

	Ble_Tl_Init();														// Initialize Ble Transport Layer
	UTIL_LPM_SetOffMode(1 << CFG_LPM_APP_BLE, UTIL_LPM_DISABLE);		// Do not allow standby in the application

	// Register the hci transport layer to handle BLE User Asynchronous Events
	UTIL_SEQ_RegTask(1 << CFG_TASK_HCI_ASYNCH_EVT_ID, UTIL_SEQ_RFU, hci_user_evt_proc);

	if (SHCI_C2_BLE_Init(&ble_init_cmd_packet) != SHCI_Success) {		// Starts the BLE Stack on CPU2
		Error_Handler();
	}

	Ble_Hci_Gap_Gatt_Init();											// Initialization of HCI & GATT & GAP layer
	SVCCTL_Init();														// Initialization of the BLE Services

	BleAppContext.connectionStatus = APP_BLE_IDLE;				// Initialization of the BLE App Context
	BleAppContext.connectionHandle = 0xFFFF;

	UTIL_SEQ_RegTask(1 << CFG_TASK_ADV_UPDATE_ID, UTIL_SEQ_RFU, Adv_Update);
	UTIL_SEQ_RegTask(1 << CFG_TASK_ADV_CANCEL_ID, UTIL_SEQ_RFU, Adv_Cancel);

	// Initialization of ADV - Ad Manufacturer Element - Support OTA Bit Mask
#if(RADIO_ACTIVITY_EVENT != 0)
	// 0x0001: Idle
	// 0x0002: Advertising
	// 0x0004: Connection event slave
	// 0x0008: Scanning
	// 0x0010: Connection request
	// 0x0020: Connection event master
	// 0x0040: TX test mode
	// 0x0080: RX test mode
	aci_hal_set_radio_activity_mask(0x0006);
#endif

	DISAPP_Init();														// Initialize DIS Application
	BAS_App_Init();														// Initialize Battery Service
	hidService.AppInit();													// Initialise HID Service

	// Create timer to handle the connection state machine
	HW_TS_Create(CFG_TIM_PROC_ID_ISR, &(BleAppContext.Advertising_mgr_timer_Id), hw_ts_SingleShot, Adv_Mgr);

	AdvIntervalMin = CFG_FAST_CONN_ADV_INTERVAL_MIN;					// Initialize intervals for reconnection without intervals update
	AdvIntervalMax = CFG_FAST_CONN_ADV_INTERVAL_MAX;

	Adv_Request(APP_BLE_FAST_ADV);										// Start to Advertise to be connected by a Client

	return;
}


SVCCTL_UserEvtFlowStatus_t SVCCTL_App_Notification(void *pckt)
{
	evt_le_meta_event *meta_evt;
	evt_blecore_aci *blecore_evt;
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
		hci_disconnection_complete_event_rp0* disconnection_complete_event = (hci_disconnection_complete_event_rp0*) event_pckt->data;

		if (disconnection_complete_event->Connection_Handle == BleAppContext.connectionHandle)	{
			BleAppContext.connectionHandle = 0;
			BleAppContext.connectionStatus = APP_BLE_IDLE;

			APP_DBG_MSG("\r\n\r** DISCONNECTION EVENT WITH CLIENT \n");
		}

		Adv_Request(APP_BLE_FAST_ADV);			// restart advertising

		HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
	}

	break;

	case HCI_LE_META_EVT_CODE:
	{
		meta_evt = (evt_le_meta_event*) event_pckt->data;

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

			ret = hci_le_read_phy(BleAppContext.connectionHandle, &TX_PHY, &RX_PHY);
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

			HW_TS_Stop(BleAppContext.Advertising_mgr_timer_Id);					// connected: no need anymore to schedule the LP ADV

			APP_DBG_MSG("HCI_LE_CONNECTION_COMPLETE_SUBEVT_CODE for connection handle 0x%x\n", connCompleteEvent->Connection_Handle);
			BleAppContext.connectionStatus = APP_BLE_CONNECTED;
			BleAppContext.connectionHandle = connCompleteEvent->Connection_Handle;

			HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
		}
		break;

		default:
			break;
		}
	}
	break;

	case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
		blecore_evt = (evt_blecore_aci*) event_pckt->data;

		switch (blecore_evt->ecode)	{
		// FIXME SPECIFIC to Custom Template APP
		case ACI_L2CAP_CONNECTION_UPDATE_RESP_VSEVT_CODE:
			break;

		case ACI_GAP_PROC_COMPLETE_VSEVT_CODE:
			APP_DBG_MSG("\r\n\r** ACI_GAP_PROC_COMPLETE_VSEVT_CODE \n");
			break;
#if(RADIO_ACTIVITY_EVENT != 0)
		case ACI_HAL_END_OF_RADIO_ACTIVITY_VSEVT_CODE:
			/* FIXME USER CODE BEGIN RADIO_ACTIVITY_EVENT*/
			HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
			HAL_Delay(5);
			HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
			/* USER CODE END RADIO_ACTIVITY_EVENT*/
			break;
#endif

		case (ACI_GAP_KEYPRESS_NOTIFICATION_VSEVT_CODE):
        	APP_DBG_MSG("\r\n\r** ACI_GAP_KEYPRESS_NOTIFICATION_VSEVT_CODE \n");
			break;

		case ACI_GAP_PASS_KEY_REQ_VSEVT_CODE:
			aci_gap_pass_key_resp(BleAppContext.connectionHandle, CFG_FIXED_PIN);
			break;

		case ACI_GAP_NUMERIC_COMPARISON_VALUE_VSEVT_CODE:
			evt_numeric_value = (aci_gap_numeric_comparison_value_event_rp0*)blecore_evt->data;
			numeric_value = evt_numeric_value->Numeric_Value;
			APP_DBG_MSG("numeric_value = %ld\n", numeric_value);
			aci_gap_numeric_comparison_value_confirm_yesno(BleAppContext.connectionHandle, YES);
			break;

		case ACI_GAP_PAIRING_COMPLETE_VSEVT_CODE:
			pairing_complete = (aci_gap_pairing_complete_event_rp0*)blecore_evt->data;
			APP_DBG_MSG("BLE_CTRL_App_Notification: ACI_GAP_PAIRING_COMPLETE_VSEVT_CODE, pairing_complete->Status = %d\n", pairing_complete->Status);
			break;
			// PAIRING
		}
		break;


		default:
			break;
	}

	return (SVCCTL_UserEvtFlowEnable);
}


APP_BLE_ConnStatus_t APP_BLE_Get_Server_Connection_Status(void)
{
	return BleAppContext.connectionStatus;
}


static void Ble_Tl_Init(void)
{
	HCI_TL_HciInitConf_t Hci_Tl_Init_Conf;

	Hci_Tl_Init_Conf.p_cmdbuffer = (uint8_t*)&BleCmdBuffer;
	Hci_Tl_Init_Conf.StatusNotCallBack = BLE_StatusNot;
	hci_init(BLE_UserEvtRx, (void*) &Hci_Tl_Init_Conf);
}


static void Ble_Hci_Gap_Gatt_Init(void)
{
	uint16_t gap_service_handle, gap_dev_name_char_handle, gap_appearance_char_handle;

	uint16_t appearance[1] = { BLE_CFG_GAP_APPEARANCE };

	// Initialize HCI layer
	hci_reset();									// HCI Reset to synchronise BLE Stack

	// Write the BD Address
	uint8_t* bd_addr = BleGetBdAddress();
	aci_hal_write_config_data(CONFIG_DATA_PUBADDR_OFFSET, CONFIG_DATA_PUBADDR_LEN, bd_addr);

	// Write Identity root key used to derive LTK and CSRK
	aci_hal_write_config_data(CONFIG_DATA_IR_OFFSET, CONFIG_DATA_IR_LEN, (uint8_t*)BLE_CFG_IR_VALUE);

	// Write Encryption root key used to derive LTK and CSRK
	aci_hal_write_config_data(CONFIG_DATA_ER_OFFSET, CONFIG_DATA_ER_LEN, (uint8_t*)BLE_CFG_ER_VALUE);

	aci_hal_set_tx_power_level(1, CFG_TX_POWER);	// Set TX Power to 0dBm.
	aci_gatt_init();								// Initialize GATT interface

	// Initialize GAP interface
	const char* name = CFG_GAP_DEVICE_NAME;
	aci_gap_init(GAP_PERIPHERAL_ROLE,
			0,
			CFG_GAP_DEVICE_NAME_LENGTH,
			&gap_service_handle,
			&gap_dev_name_char_handle,
			&gap_appearance_char_handle);

	if (aci_gatt_update_char_value(gap_service_handle, gap_dev_name_char_handle, 0, strlen(name), (uint8_t*)name)) {
		BLE_DBG_SVCCTL_MSG("Device Name aci_gatt_update_char_value failed.\n");
	}

	if (aci_gatt_update_char_value(gap_service_handle, gap_appearance_char_handle, 0, 2, (uint8_t*)&appearance)) {
		BLE_DBG_SVCCTL_MSG("Appearance aci_gatt_update_char_value failed.\n");
	}

	// Initialize Default PHY
	hci_le_set_default_phy(ALL_PHYS_PREFERENCE, TX_2M_PREFERRED, RX_2M_PREFERRED);

	// Initialize IO capability
	aci_gap_set_io_capability(BleAppContext.bleSecurityParam.ioCapability);

	// Initialize authentication
	aci_gap_set_authentication_requirement(BleAppContext.bleSecurityParam.bonding_mode,
			BleAppContext.bleSecurityParam.mitm_mode,
			CFG_SC_SUPPORT,
			CFG_KEYPRESS_NOTIFICATION_SUPPORT,
			BleAppContext.bleSecurityParam.encryptionKeySizeMin,
			BleAppContext.bleSecurityParam.encryptionKeySizeMax,
			BleAppContext.bleSecurityParam.Use_Fixed_Pin,
			BleAppContext.bleSecurityParam.Fixed_Pin,
			CFG_BLE_ADDRESS_TYPE
	);

	// Initialize whitelist
	if (BleAppContext.bleSecurityParam.bonding_mode) {
		aci_gap_configure_whitelist();
	}
}


// Start to Advertise to be connected by a Client
static void Adv_Request(APP_BLE_ConnStatus_t New_Status)
{
	tBleStatus ret = BLE_STATUS_INVALID_PARAMS;
	//uint16_t Min_Inter, Max_Inter;

//	if (New_Status == APP_BLE_FAST_ADV)	{
//		Min_Inter = AdvIntervalMin;
//		Max_Inter = AdvIntervalMax;
//	} else {
//		Min_Inter = CFG_LP_CONN_ADV_INTERVAL_MIN;
//		Max_Inter = CFG_LP_CONN_ADV_INTERVAL_MAX;
//	}

	// Stop the timer, it will be restarted for a new shot; It does not hurt if the timer was not running
	HW_TS_Stop(BleAppContext.Advertising_mgr_timer_Id);

	APP_DBG_MSG("BLE - Current connection status: %d\n", BleAppContext.connectionStatus);

	if ((New_Status == APP_BLE_LP_ADV)
			&& ((BleAppContext.connectionStatus == APP_BLE_FAST_ADV)
					|| (BleAppContext.connectionStatus == APP_BLE_LP_ADV)))
	{
		// Connection in ADVERTISE mode have to stop the current advertising
		ret = aci_gap_set_non_discoverable();
		if (ret == BLE_STATUS_SUCCESS) {
			APP_DBG_MSG("Successfully Stopped Advertising \n");
		} else {
			APP_DBG_MSG("Stop Advertising Failed , result: %d \n", ret);
		}
	}

	BleAppContext.connectionStatus = New_Status;
	// Start Fast or Low Power Advertising
	ret = aci_gap_set_discoverable(
			ADV_TYPE,
			CFG_FAST_CONN_ADV_INTERVAL_MIN,
			CFG_FAST_CONN_ADV_INTERVAL_MAX,
			CFG_BLE_ADDRESS_TYPE,
			ADV_FILTER,
			0,
			0,
			0,
			0,
			0,
			0);

	// Update Advertising data
	ret = aci_gap_update_adv_data(sizeof(ad_data), (uint8_t*) ad_data);

	if (ret == BLE_STATUS_SUCCESS) {
		if (New_Status == APP_BLE_FAST_ADV)	{
			APP_DBG_MSG("Successfully Start Fast Advertising \n");
			// Start Timer to STOP ADV - TIMEOUT
			HW_TS_Start(BleAppContext.Advertising_mgr_timer_Id, INITIAL_ADV_TIMEOUT);
		} else {
			APP_DBG_MSG("Successfully Start Low Power Advertising \n");
		}
	} else {
		if (New_Status == APP_BLE_FAST_ADV) {
			APP_DBG_MSG("Start Fast Advertising Failed , result: %d \n", ret);
		} else {
			APP_DBG_MSG("Start Low Power Advertising Failed , result: %d \n", ret);
		}
	}

	return;
}


uint8_t* BleGetBdAddress()
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


static void Adv_Mgr()
{
	// The code shall be executed in the background as an aci command may be sent
	// The background is the only place where the application can make sure a new aci command is not sent if there is a pending one
	UTIL_SEQ_SetTask(1 << CFG_TASK_ADV_UPDATE_ID, CFG_SCH_PRIO_0);
}

static void Adv_Update()
{
	Adv_Request(APP_BLE_LP_ADV);
}


static void Adv_Cancel()
{
	if (BleAppContext.connectionStatus != APP_BLE_CONNECTED) {

		tBleStatus result = aci_gap_set_non_discoverable();

		BleAppContext.connectionStatus = APP_BLE_IDLE;
		if (result == BLE_STATUS_SUCCESS) {
			APP_DBG_MSG("  \r\n\r");APP_DBG_MSG("** STOP ADVERTISING **  \r\n\r");
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

static void BLE_UserEvtRx(void * pPayload)
{
	tHCI_UserEvtRxParam* pParam = (tHCI_UserEvtRxParam*)pPayload;
	SVCCTL_UserEvtFlowStatus_t svctl_return_status = SVCCTL_UserEvtRx((void*)&(pParam->pckt->evtserial));

	if (svctl_return_status != SVCCTL_UserEvtFlowDisable) {
		pParam->status = HCI_TL_UserEventFlow_Enable;
	} else {
		pParam->status = HCI_TL_UserEventFlow_Disable;
	}
}


static void BLE_StatusNot(HCI_TL_CmdStatus_t status)
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

