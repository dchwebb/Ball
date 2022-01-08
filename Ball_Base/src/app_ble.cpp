#include "app_ble.h"
#include "ble_hid.h"
#include "main.h"
#include "dbg_trace.h"
#include "ble.h"
#include "stm32_seq.h"
#include "shci.h"
#include "stm32_lpm.h"
#include "uartHandler.h"
#include "usb.h"
#include <bitset>

extern USBHandler usb;
extern HidApp hidApp;

PLACE_IN_SECTION("MB_MEM1") ALIGN(4) static TL_CmdPacket_t BleCmdBuffer;


void BleApp::Init()
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
	TransportLayerInit();

	// Do not allow standby in the application
	UTIL_LPM_SetOffMode(1 << CFG_LPM_APP_BLE, UTIL_LPM_DISABLE);

	// Register the hci transport layer to handle BLE User Asynchronous Events
	UTIL_SEQ_RegTask(1 << CFG_TASK_HCI_ASYNCH_EVT_ID, UTIL_SEQ_RFU, hci_user_evt_proc);

	// Starts the BLE Stack on CPU2
	if (SHCI_C2_BLE_Init(&ble_init_cmd_packet) != SHCI_Success) {
		Error_Handler();
	}

	// Initialization of HCI & GATT & GAP layer
	HciGapGattInit();

	// Initialization of the BLE Services
	SVCCTL_Init();

	// From here, all initialization are BLE application specific
	UTIL_SEQ_RegTask(1 << CFG_TASK_START_SCAN_ID, UTIL_SEQ_RFU, ScanRequest);
	UTIL_SEQ_RegTask(1 << CFG_TASK_CONN_DEV_1_ID, UTIL_SEQ_RFU, ConnectRequest);
	UTIL_SEQ_RegTask(1 << CFG_TASK_SW1_BUTTON_PUSHED_ID, UTIL_SEQ_RFU, DisconnectRequest);

	// Initialization of the BLE App Context
	bleApp.deviceConnectionStatus = ConnectionStatus::Idle;

	// Radio mask Activity
	aci_hal_set_radio_activity_mask(0x0020);			// Connection event master

	// Initialize HID Client Application
	hidApp.Init();

}


SVCCTL_UserEvtFlowStatus_t SVCCTL_App_Notification(void* pckt)
{
	bleApp.ServiceControlCallback(pckt);
	return SVCCTL_UserEvtFlowEnable;
}


void BleApp::ServiceControlCallback(void* pckt)
{
	hci_event_pckt* event_pckt = (hci_event_pckt*) ((hci_uart_pckt*) pckt)->data;
	uint8_t result;

	switch (event_pckt->evt)
	{

		case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
		{
			evt_blecore_aci* blecore_evt = (evt_blecore_aci*)event_pckt->data;

			switch (blecore_evt->ecode)
			{

				case ACI_GAP_PROC_COMPLETE_VSEVT_CODE:
				{
					aci_gap_proc_complete_event_rp0 *gap_evt_proc_complete = (aci_gap_proc_complete_event_rp0*) blecore_evt->data;

					if (gap_evt_proc_complete->Procedure_Code == GAP_GENERAL_DISCOVERY_PROC	&& gap_evt_proc_complete->Status == 0x00) {

						APP_DBG_MSG("* BLE: GAP General discovery procedure completed\r\n");
						// if a device found, connect to it, device 1 being chosen first if both found
						if (action == RequestAction::ScanConnect && deviceServerFound && deviceConnectionStatus != BleApp::ConnectionStatus::ClientConnected) {
							UTIL_SEQ_SetTask(1 << CFG_TASK_CONN_DEV_1_ID, CFG_SCH_PRIO_0);
						}
					}
				}
				break;

				case ACI_L2CAP_CONNECTION_UPDATE_REQ_VSEVT_CODE:
				{
					aci_l2cap_connection_update_req_event_rp0 *pr = (aci_l2cap_connection_update_req_event_rp0*) blecore_evt->data;
					aci_hal_set_radio_activity_mask(0x0000);

					result = aci_l2cap_connection_parameter_update_resp(connectionHandle,
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
			if (cc->Connection_Handle == connectionHandle) {
				connectionHandle = 0;
				deviceConnectionStatus = BleApp::ConnectionStatus::Idle;
				APP_DBG_MSG("* BLE: Disconnection event with server\r\n");
				hidApp.HIDConnectionNotification();
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
					connectionHandle = connection_complete_event->Connection_Handle;
					deviceConnectionStatus = BleApp::ConnectionStatus::ClientConnected;

					// connection with client: notify HidApp
					APP_DBG_MSG("* BLE: Connection event with server\r\n");

					if (action == RequestAction::GetReportMap) {
						hidApp.action = HidApp::HidAction::GetReportMap;
					} else {
						hidApp.action = HidApp::HidAction::Connect;
					}
					hidApp.HIDConnectionNotification();

					result = aci_gatt_disc_all_primary_services(connectionHandle);
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

					std::unique_ptr<AdvertisingReport> ar = std::make_unique<AdvertisingReport>();

					memcpy(ar->address, le_advertising_event->Advertising_Report[0].Address, bdddrSize);

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
									ar->flags = adv_report_data[k + 2];
									break;

								case AD_TYPE_16_BIT_SERV_UUID_CMPLT_LIST:
									ar->serviceClasses = *(uint16_t*)&adv_report_data[k + 2];
									if (action == RequestAction::ScanConnect && ar->serviceClasses == 0x1812) {	// FIXME whitelist etc
										APP_DBG_MSG("* BLE: Server detected - HID Device\r\n");
										deviceServerFound = true;
										memcpy(&remoteConnectAddress, le_advertising_event->Advertising_Report[0].Address, bdddrSize);
										aci_gap_terminate_gap_proc(0x2);		// If connecting terminate scan
									}
									break;

								case AD_TYPE_SHORTENED_LOCAL_NAME:
									ar->shortName = std::string ((char*)&adv_report_data[k + 2], adlength - 1);
									break;

								case AD_TYPE_TX_POWER_LEVEL:
									break;

								case AD_TYPE_SERVICE_DATA:
									break;

								case AD_TYPE_APPEARANCE:
									ar->appearance = *(uint16_t*)&adv_report_data[k + 2];
									break;

								case AD_TYPE_MANUFACTURER_SPECIFIC_DATA:
									ar->manufactLen = std::min((unsigned int)adlength, sizeof ar->manufactData);
									memcpy(ar->manufactData, &adv_report_data[k + 2], ar->manufactLen);
									break;

								default:
									break;
							}
							k += adlength + 1;
						}
						if (action == RequestAction::ScanInfo) {
							PrintAdvData(std::move(ar));
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

}


void BleApp::PrintAdvData(std::unique_ptr<AdvertisingReport> ar)
{
	advMsg = "* BLE: Found device:"
			"\r\n  Address: " + ar->formatAddress() +
			"\r\n  Flags: " + std::bitset<8>(ar->flags).to_string() +
			"\r\n  Name: " + ar->shortName +
			"\r\n  Appearance: " + HexByte(ar->appearance) +
			"\r\n  Service class: " + HexByte(ar->serviceClasses) +
			"\r\n  Manufacturer data: " + HexToString(ar->manufactData, ar->manufactLen, true) +
			"\r\n";
	usb.SendString(advMsg);
}


BleApp::ConnectionStatus BleApp::GetClientConnectionStatus(uint16_t connHandle)
{
	if (connectionHandle == connHandle) {
		return deviceConnectionStatus;
	}
	return ConnectionStatus::Idle;
}


void BleApp::ScanInfo()
{
	if (bleApp.deviceConnectionStatus == ConnectionStatus::Idle) {
		bleApp.action = RequestAction::ScanInfo;
		UTIL_SEQ_SetTask(1 << CFG_TASK_START_SCAN_ID, CFG_SCH_PRIO_0);
	} else {
		APP_DBG_MSG("Connection is busy ...\n");
	}
}


void BleApp::GetHidReportMap(uint8_t* address)
{
	if (bleApp.deviceConnectionStatus == ConnectionStatus::Idle) {
		bleApp.action = RequestAction::GetReportMap;
		memcpy(&remoteConnectAddress, address, bdddrSize);
		UTIL_SEQ_SetTask(1 << CFG_TASK_CONN_DEV_1_ID, CFG_SCH_PRIO_0);
	} else {
		APP_DBG_MSG("Connection is busy ...\n");
	}
}


void BleApp::ScanAndConnect()
{
	if (hidApp.state != HidApp::HidState::ClientConnected)	{
		bleApp.action = RequestAction::ScanConnect;
		UTIL_SEQ_SetTask(1 << CFG_TASK_START_SCAN_ID, CFG_SCH_PRIO_0);
	} else {
		UTIL_SEQ_SetTask(1 << CFG_TASK_SW1_BUTTON_PUSHED_ID, CFG_SCH_PRIO_0);

		//HID_APP_SW1_Button_Action();
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
	const char *deviceName = "MJOY_CL";
	uint16_t gap_service_handle, gap_dev_name_char_handle, gap_appearance_char_handle;
	uint32_t srd_bd_addr[2];
	uint16_t appearance[1] = {0};

	// HCI Reset to synchronise BLE Stack
	hci_reset();

	// Write the BD Address
	aci_hal_write_config_data(CONFIG_DATA_PUBADDR_OFFSET, CONFIG_DATA_PUBADDR_LEN, GetBdAddress());

	// Static random Address: two upper bits set to 1, lowest 32bits read from UDN to differentiate between devices
	// The RNG may be used to provide a random number on each power on
	srd_bd_addr[1] =  0x0000ED6E;
	srd_bd_addr[0] =  LL_FLASH_GetUDN();
	aci_hal_write_config_data(CONFIG_DATA_RANDOM_ADDRESS_OFFSET, CONFIG_DATA_RANDOM_ADDRESS_LEN, (uint8_t*)srd_bd_addr);

	// Write Identity root key used to derive LTK and CSRK
	aci_hal_write_config_data(CONFIG_DATA_IR_OFFSET, CONFIG_DATA_IR_LEN, (uint8_t*)IdentityRootKey);

	// Write Encryption root key used to derive LTK and CSRK
	aci_hal_write_config_data(CONFIG_DATA_ER_OFFSET, CONFIG_DATA_ER_LEN, (uint8_t*)EncryptionRootKey);

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


void BleApp::ScanRequest()
{
	if (bleApp.deviceConnectionStatus != ConnectionStatus::ClientConnected) {
		bleApp.deviceServerFound = false;
		tBleStatus result = aci_gap_start_general_discovery_proc(SCAN_P, SCAN_L, PUBLIC_ADDR, 1);
		if (result == BLE_STATUS_SUCCESS) {
			APP_DBG_MSG("* BLE: Start general discovery\r\n");
		} else {
			APP_DBG_MSG("* BLE: aci_gap_start_general_discovery_proc, Failed \r\n");
		}
	}
}


void BleApp::ConnectRequest()
{
	APP_DBG_MSG("* BLE: Create connection to server\r\n");

	if (bleApp.deviceConnectionStatus != ConnectionStatus::ClientConnected) {
		tBleStatus result = aci_gap_create_connection(SCAN_P,
				SCAN_L,
				RANDOM_ADDR, bleApp.remoteConnectAddress,
				PUBLIC_ADDR,
				CONN_P1,
				CONN_P2,
				0,
				SUPERV_TIMEOUT,
				CONN_L1,
				CONN_L2);

		if (result == BLE_STATUS_SUCCESS) {
			bleApp.deviceConnectionStatus = ConnectionStatus::Connecting;
		} else {
			bleApp.deviceConnectionStatus = ConnectionStatus::Idle;
		}
	}
}


void BleApp::DisconnectRequest()
{
	aci_gap_terminate(bleApp.connectionHandle, 0x16);			// 0x16: Connection Terminated by Local Host
}


uint8_t* BleApp::GetBdAddress()
{
	// generates ble address fron data in flash
	uint32_t udn = LL_FLASH_GetUDN();
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
	return bd_addr_udn;
}


void BleApp::UserEvtRx(void* pPayload)
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
