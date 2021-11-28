#include "ble_hid.h"
#include "main.h"
#include "app_common.h"
#include "dbg_trace.h"
#include "ble.h"
#include "tl.h"
#include "app_ble.h"
#include "stm32_seq.h"
#include "shci.h"
#include "stm32_lpm.h"
#include "otp.h"
#include "uartHandler.h"

typedef struct {
	uint16_t connectionHandle;			// connection handle of the current active connection; When not in connection, the handle is set to 0xFFFF
	APP_BLE_ConnStatus_t Device_Connection_Status;
	uint8_t DeviceServerFound;
} BleApplicationContext_t;

#define BLE_CFG_GAP_APPEARANCE 0
#define BD_ADDR_SIZE_LOCAL    6

PLACE_IN_SECTION("MB_MEM1") ALIGN(4) static TL_CmdPacket_t BleCmdBuffer;

static const uint8_t M_bd_addr[BD_ADDR_SIZE_LOCAL] = {
		(uint8_t)((CFG_ADV_BD_ADDRESS & 0x0000000000FF)),
		(uint8_t)((CFG_ADV_BD_ADDRESS & 0x00000000FF00) >> 8),
		(uint8_t)((CFG_ADV_BD_ADDRESS & 0x000000FF0000) >> 16),
		(uint8_t)((CFG_ADV_BD_ADDRESS & 0x0000FF000000) >> 24),
		(uint8_t)((CFG_ADV_BD_ADDRESS & 0x00FF00000000) >> 32),
		(uint8_t)((CFG_ADV_BD_ADDRESS & 0xFF0000000000) >> 40)
};

static uint8_t bd_addr_udn[BD_ADDR_SIZE_LOCAL];

static const uint8_t BLE_CFG_IR_VALUE[16] = CFG_BLE_IRK;		//  Identity root key used to derive LTK and CSRK
static const uint8_t BLE_CFG_ER_VALUE[16] = CFG_BLE_ERK;		// Encryption root key used to derive LTK and CSRK
tBDAddr SERVER_REMOTE_BDADDR;
P2PC_APP_ConnHandle_Not_evt_t handleNotification;

PLACE_IN_SECTION("BLE_APP_CONTEXT") static BleApplicationContext_t BleApplicationContext;


// Private function prototypes
static void BLE_UserEvtRx(void* pPayload);
static void BLE_StatusNot(HCI_TL_CmdStatus_t status);
static void Ble_Tl_Init();
static void Ble_Hci_Gap_Gatt_Init();
static const uint8_t* BleGetBdAddress();
static void Scan_Request();
static void Connect_Request();
static void DisconnectRequest();


void APP_BLE_Init()
{
	SHCI_C2_Ble_Init_Cmd_Packet_t ble_init_cmd_packet =	{
			{{0,0,0}},                          // Header unused
			{0,                                 // pBleBufferAddress not used
					0,                                  // BleBufferSize not used
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


	// Initialize Ble Transport Layer
	Ble_Tl_Init();

	// Do not allow standby in the application
	UTIL_LPM_SetOffMode(1 << CFG_LPM_APP_BLE, UTIL_LPM_DISABLE);

	// Register the hci transport layer to handle BLE User Asynchronous Events
	UTIL_SEQ_RegTask(1 << CFG_TASK_HCI_ASYNCH_EVT_ID, UTIL_SEQ_RFU, hci_user_evt_proc);

	// Starts the BLE Stack on CPU2
	if (SHCI_C2_BLE_Init(&ble_init_cmd_packet) != SHCI_Success) {
		Error_Handler();
	}

	// Initialization of HCI & GATT & GAP layer
	Ble_Hci_Gap_Gatt_Init();

	// Initialization of the BLE Services
	SVCCTL_Init();

	// From here, all initialization are BLE application specific
	UTIL_SEQ_RegTask(1 << CFG_TASK_START_SCAN_ID, UTIL_SEQ_RFU, Scan_Request);
	UTIL_SEQ_RegTask(1 << CFG_TASK_CONN_DEV_1_ID, UTIL_SEQ_RFU, Connect_Request);
	UTIL_SEQ_RegTask(1 << CFG_TASK_SW1_BUTTON_PUSHED_ID, UTIL_SEQ_RFU, DisconnectRequest);

	// Initialization of the BLE App Context
	BleApplicationContext.Device_Connection_Status = APP_BLE_IDLE;

	// Radio mask Activity
	aci_hal_set_radio_activity_mask(0x0020);			// Connection event master

	// Initialize HID Client Application
	HID_APP_Init();

	return;
}

SVCCTL_UserEvtFlowStatus_t SVCCTL_App_Notification(void* pckt)
{
	hci_event_pckt* event_pckt = (hci_event_pckt*) ((hci_uart_pckt*) pckt)->data;
	uint8_t result;

	switch (event_pckt->evt)
	{

		case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
		{
			handleNotification.P2P_Evt_Opcode = PEER_DISCON_HANDLE_EVT;
			evt_blecore_aci* blecore_evt = (evt_blecore_aci*)event_pckt->data;

			switch (blecore_evt->ecode)
			{

				case ACI_GAP_PROC_COMPLETE_VSEVT_CODE:
				{
					aci_gap_proc_complete_event_rp0 *gap_evt_proc_complete = (aci_gap_proc_complete_event_rp0*) blecore_evt->data;

					if (gap_evt_proc_complete->Procedure_Code == GAP_GENERAL_DISCOVERY_PROC	&& gap_evt_proc_complete->Status == 0x00) {

						APP_DBG_MSG("* BLE: GAP General discovery procedure completed\r\n");
						// if a device found, connect to it, device 1 being chosen first if both found
						if (BleApplicationContext.DeviceServerFound == 0x01 && BleApplicationContext.Device_Connection_Status != APP_BLE_CONNECTED_CLIENT) {
							UTIL_SEQ_SetTask(1 << CFG_TASK_CONN_DEV_1_ID, CFG_SCH_PRIO_0);
						}
					}
				}
				break;

				case ACI_L2CAP_CONNECTION_UPDATE_REQ_VSEVT_CODE:
				{
					aci_l2cap_connection_update_req_event_rp0 *pr = (aci_l2cap_connection_update_req_event_rp0*) blecore_evt->data;
					aci_hal_set_radio_activity_mask(0x0000);

					result = aci_l2cap_connection_parameter_update_resp(BleApplicationContext.connectionHandle,
							pr->Interval_Min,
							pr->Interval_Max,
							pr->Slave_Latency,
							pr->Timeout_Multiplier,
							CONN_L1,
							CONN_L2,
							pr->Identifier,
							0x01);

					aci_hal_set_radio_activity_mask(0x0020);
				}
				break;

				default:
					break;
				}
			}
		break;

		case HCI_DISCONNECTION_COMPLETE_EVT_CODE:
		{
			hci_disconnection_complete_event_rp0* cc = (hci_disconnection_complete_event_rp0*) event_pckt->data;
			if (cc->Connection_Handle == BleApplicationContext.connectionHandle) {
				BleApplicationContext.connectionHandle = 0;
				BleApplicationContext.Device_Connection_Status = APP_BLE_IDLE;
				APP_DBG_MSG("* BLE: Disconnection event with server\r\n");
				handleNotification.P2P_Evt_Opcode = PEER_DISCON_HANDLE_EVT;
				handleNotification.ConnectionHandle = BleApplicationContext.connectionHandle;
				HIDConnectionNotification(&handleNotification);
			}
		}
		break;

		case HCI_LE_META_EVT_CODE:
		{
			evt_le_meta_event* meta_evt = (evt_le_meta_event*) event_pckt->data;

			switch (meta_evt->subevent)
			{
				case HCI_LE_CONNECTION_COMPLETE_SUBEVT_CODE:
				{
					// The connection is done
					hci_le_connection_complete_event_rp0* connection_complete_event = (hci_le_connection_complete_event_rp0*) meta_evt->data;
					BleApplicationContext.connectionHandle = connection_complete_event->Connection_Handle;
					BleApplicationContext.Device_Connection_Status = APP_BLE_CONNECTED_CLIENT;

					// connection with client
					APP_DBG_MSG("* BLE: Connection event with server\r\n");
					handleNotification.P2P_Evt_Opcode = PEER_CONN_HANDLE_EVT;
					handleNotification.ConnectionHandle = BleApplicationContext.connectionHandle;
					HIDConnectionNotification(&handleNotification);

					result = aci_gatt_disc_all_primary_services(BleApplicationContext.connectionHandle);
					if (result == BLE_STATUS_SUCCESS) {
						APP_DBG_MSG("* BLE: Start Searching Primary Services\r\n");
					} else {
						APP_DBG_MSG("* BLE: All services discovery Failed\r\n");
					}

					break;
				}

				case HCI_LE_ADVERTISING_REPORT_SUBEVT_CODE:
				{
					hci_le_advertising_report_event_rp0* le_advertising_event = (hci_le_advertising_report_event_rp0*) meta_evt->data;
					uint8_t event_type = le_advertising_event->Advertising_Report[0].Event_Type;
					uint8_t event_data_size = le_advertising_event->Advertising_Report[0].Length_Data;

					// When decoding advertising report the data and RSSI values must be read by using offsets
					// RSSI = (int8_t)*(uint8_t*) (adv_report_data + le_advertising_event->Advertising_Report[0].Length_Data);
					uint8_t* adv_report_data = (uint8_t*)(&le_advertising_event->Advertising_Report[0].Length_Data) + 1;
					int k = 0;

					// search AD Type for HID Devices
					if (event_type == ADV_IND) {

						while (k < event_data_size) {
							uint8_t adlength = adv_report_data[k];
							uint8_t adtype = adv_report_data[k + 1];
							switch (adtype)
							{
								case AD_TYPE_FLAGS:
									break;

								case AD_TYPE_TX_POWER_LEVEL:
									break;

								case AD_TYPE_MANUFACTURER_SPECIFIC_DATA:
									break;

								case AD_TYPE_SERVICE_DATA:
									break;

								case AD_TYPE_16_BIT_SERV_UUID_CMPLT_LIST:

									if (adlength == 3 && (*(uint16_t*)&adv_report_data[k + 2]) == 0x1812) {
										APP_DBG_MSG("* BLE: Server detected - HID Device\r\n");
										BleApplicationContext.DeviceServerFound = 0x01;
										SERVER_REMOTE_BDADDR[0] = le_advertising_event->Advertising_Report[0].Address[0];
										SERVER_REMOTE_BDADDR[1] = le_advertising_event->Advertising_Report[0].Address[1];
										SERVER_REMOTE_BDADDR[2] = le_advertising_event->Advertising_Report[0].Address[2];
										SERVER_REMOTE_BDADDR[3] = le_advertising_event->Advertising_Report[0].Address[3];
										SERVER_REMOTE_BDADDR[4] = le_advertising_event->Advertising_Report[0].Address[4];
										SERVER_REMOTE_BDADDR[5] = le_advertising_event->Advertising_Report[0].Address[5];
										aci_gap_terminate_gap_proc(0x2);		// Terminate scan
									}
									break;

								default:
									break;
							}
							k += adlength + 1;
						}
					}
				}

				break;

				default:
					break;
			}
		}
		break; /* HCI_LE_META_EVT_CODE */

		default:
			break;
	}

	return (SVCCTL_UserEvtFlowEnable);
}


APP_BLE_ConnStatus_t APP_BLE_Get_Client_Connection_Status(uint16_t Connection_Handle)
{
	if (BleApplicationContext.connectionHandle == Connection_Handle) {
		return BleApplicationContext.Device_Connection_Status;
	}
	return APP_BLE_IDLE;
}


void APP_BLE_Key_Button1_Action()
{
	if (HID_Client_APP_Get_State() != APP_BLE_CONNECTED_CLIENT)	{
		UTIL_SEQ_SetTask(1 << CFG_TASK_START_SCAN_ID, CFG_SCH_PRIO_0);
	} else {
		UTIL_SEQ_SetTask(1 << CFG_TASK_SW1_BUTTON_PUSHED_ID, CFG_SCH_PRIO_0);

		//HID_APP_SW1_Button_Action();
	}
}

void APP_BLE_Key_Button2_Action()
{
	uartSendString("Button 2 longer message testing \r\n");
}


void APP_BLE_Key_Button3_Action()
{
	printf("Button 3 \r\n");
}


static void Ble_Tl_Init()
{
	HCI_TL_HciInitConf_t Hci_Tl_Init_Conf;

	Hci_Tl_Init_Conf.p_cmdbuffer = (uint8_t*)&BleCmdBuffer;
	Hci_Tl_Init_Conf.StatusNotCallBack = BLE_StatusNot;
	hci_init(BLE_UserEvtRx, (void*) &Hci_Tl_Init_Conf);
}

static void Ble_Hci_Gap_Gatt_Init()
{
	const char *deviceName = "MJOY_CL";
	uint16_t gap_service_handle, gap_dev_name_char_handle, gap_appearance_char_handle;
	uint32_t srd_bd_addr[2];
	uint16_t appearance[1] = { BLE_CFG_GAP_APPEARANCE };

	// Initialize HCI layer

	// HCI Reset to synchronise BLE Stack
	hci_reset();

	// Write the BD Address
	const uint8_t* bd_addr = BleGetBdAddress();
	aci_hal_write_config_data(CONFIG_DATA_PUBADDR_OFFSET, CONFIG_DATA_PUBADDR_LEN, (uint8_t*)bd_addr);

	// Static random Address: two upper bits set to 1, lowest 32bits read from UDN to differentiate between devices
	// The RNG may be used to provide a random number on each power on
	srd_bd_addr[1] =  0x0000ED6E;
	srd_bd_addr[0] =  LL_FLASH_GetUDN();
	aci_hal_write_config_data(CONFIG_DATA_RANDOM_ADDRESS_OFFSET, CONFIG_DATA_RANDOM_ADDRESS_LEN, (uint8_t*)srd_bd_addr);

	// Write Identity root key used to derive LTK and CSRK
	aci_hal_write_config_data(CONFIG_DATA_IR_OFFSET, CONFIG_DATA_IR_LEN, (uint8_t*)BLE_CFG_IR_VALUE);

	// Write Encryption root key used to derive LTK and CSRK
	aci_hal_write_config_data(CONFIG_DATA_ER_OFFSET, CONFIG_DATA_ER_LEN, (uint8_t*)BLE_CFG_ER_VALUE);

	// Set TX Power to 0dBm.
	aci_hal_set_tx_power_level(1, CFG_TX_POWER);

	// Initialize GATT interface
	aci_gatt_init();

	// Initialize GAP interface
	aci_gap_init(GAP_CENTRAL_ROLE, 0, strlen(deviceName), &gap_service_handle, &gap_dev_name_char_handle, &gap_appearance_char_handle);

	if (aci_gatt_update_char_value(gap_service_handle, gap_dev_name_char_handle, 0, strlen(deviceName), (uint8_t*)deviceName)) {
		BLE_DBG_SVCCTL_MSG("Device Name aci_gatt_update_char_value failed.\n");
	}

	if (aci_gatt_update_char_value(gap_service_handle, gap_appearance_char_handle, 0, 2, (uint8_t*)&appearance)) {
		BLE_DBG_SVCCTL_MSG("Appearance aci_gatt_update_char_value failed.\n");
	}

	// Initialize IO capability
	aci_gap_set_io_capability(CFG_IO_CAPABILITY);

	// Initialize authentication
	aci_gap_set_authentication_requirement(CFG_BONDING_MODE,
			CFG_MITM_PROTECTION,
			CFG_SC_SUPPORT,
			CFG_KEYPRESS_NOTIFICATION_SUPPORT,
			CFG_ENCRYPTION_KEY_SIZE_MIN,
			CFG_ENCRYPTION_KEY_SIZE_MAX,
			CFG_USED_FIXED_PIN,
			CFG_FIXED_PIN,
			PUBLIC_ADDR
			);

	// Initialize whitelist
	if (CFG_BONDING_MODE) {
		aci_gap_configure_whitelist();
	}
}


static void Scan_Request()
{
	if (BleApplicationContext.Device_Connection_Status != APP_BLE_CONNECTED_CLIENT) {
		tBleStatus result = aci_gap_start_general_discovery_proc(SCAN_P, SCAN_L, PUBLIC_ADDR, 1);
		if (result == BLE_STATUS_SUCCESS) {
			APP_DBG_MSG("* BLE: Start general discovery\r\n");
		} else {
			APP_DBG_MSG("* BLE: aci_gap_start_general_discovery_proc, Failed \r\n");
		}
	}
}


static void Connect_Request()
{
	APP_DBG_MSG("* BLE: Create connection to server\r\n");

	if (BleApplicationContext.Device_Connection_Status != APP_BLE_CONNECTED_CLIENT) {
		tBleStatus result = aci_gap_create_connection(SCAN_P,
				SCAN_L,
				RANDOM_ADDR, SERVER_REMOTE_BDADDR,
				PUBLIC_ADDR,
				CONN_P1,
				CONN_P2,
				0,
				SUPERV_TIMEOUT,
				CONN_L1,
				CONN_L2);

		if (result == BLE_STATUS_SUCCESS) {
			BleApplicationContext.Device_Connection_Status = APP_BLE_LP_CONNECTING;
		} else {
			BleApplicationContext.Device_Connection_Status = APP_BLE_IDLE;
		}
	}
	return;
}


static void DisconnectRequest()
{
	aci_gap_terminate(BleApplicationContext.connectionHandle, 0x16);			// 0x16: Connection Terminated by Local Host
}


const uint8_t* BleGetBdAddress()
{
	const uint8_t* bd_addr;
	uint32_t udn = LL_FLASH_GetUDN();

	if (udn != 0xFFFFFFFF) {
		uint32_t company_id = LL_FLASH_GetSTCompanyID();
		uint32_t device_id = LL_FLASH_GetDeviceID();

		// Public Address with the ST company ID
		// bit[47:24] : 24bits (OUI) equal to the company ID
		// bit[23:16] : Device ID.
		// bit[15:0] : The last 16bits from the UDN
		// Note: In order to use the Public Address in a final product, a dedicated 24bit company ID (OUI) shall be bought.
		bd_addr_udn[0] = (uint8_t)(udn & 0x000000FF);
		bd_addr_udn[1] = (uint8_t)((udn & 0x0000FF00) >> 8 );
		bd_addr_udn[2] = (uint8_t)device_id;
		bd_addr_udn[3] = (uint8_t)(company_id & 0x000000FF);
		bd_addr_udn[4] = (uint8_t)((company_id & 0x0000FF00) >> 8 );
		bd_addr_udn[5] = (uint8_t)((company_id & 0x00FF0000) >> 16 );

		bd_addr = (const uint8_t*)bd_addr_udn;
	} else {
		uint8_t* otp_addr = OTP_Read(0);

		if (otp_addr) {
			bd_addr = ((OTP_ID0_t*)otp_addr)->bd_address;
		} else {
			bd_addr = M_bd_addr;
		}
	}

	return bd_addr;
}


void hci_notify_asynch_evt(void* pdata)
{
	UTIL_SEQ_SetTask(1 << CFG_TASK_HCI_ASYNCH_EVT_ID, CFG_SCH_PRIO_0);
	return;
}


void hci_cmd_resp_release(uint32_t flag)
{
	UTIL_SEQ_SetEvt(1 << CFG_IDLEEVT_HCI_CMD_EVT_RSP_ID);
	return;
}


void hci_cmd_resp_wait(uint32_t timeout)
{
	UTIL_SEQ_WaitEvt(1 << CFG_IDLEEVT_HCI_CMD_EVT_RSP_ID);
	return;
}


static void BLE_UserEvtRx(void* pPayload)
{
	tHCI_UserEvtRxParam* pParam = (tHCI_UserEvtRxParam*)pPayload;

	SVCCTL_UserEvtFlowStatus_t svctl_return_status = SVCCTL_UserEvtRx((void*)&(pParam->pckt->evtserial));
	if (svctl_return_status != SVCCTL_UserEvtFlowDisable) {
		pParam->status = HCI_TL_UserEventFlow_Enable;
	} else {
		pParam->status = HCI_TL_UserEventFlow_Disable;
	}
}


static void BLE_StatusNot( HCI_TL_CmdStatus_t status )
{
	uint32_t task_id_list;
	switch (status)
	{
		case HCI_TL_CmdBusy:
			// All tasks that may send an aci/hci commands shall be listed here to prevent a new command is sent while one is already pending
			task_id_list = (1 << CFG_LAST_TASK_ID_WITH_HCICMD) - 1;
			UTIL_SEQ_PauseTask(task_id_list);
			break;

		case HCI_TL_CmdAvailable:
			// All tasks that may send an aci/hci commands shall be listed here to prevent a new command is sent while one is already pending
			task_id_list = (1 << CFG_LAST_TASK_ID_WITH_HCICMD) - 1;
			UTIL_SEQ_ResumeTask(task_id_list);
			break;

		default:
			break;
	}
}


void SVCCTL_ResumeUserEventFlow()
{
	hci_resume_flow();
}

