#include <BleApp.h>
#include <HidApp.h>
#include "ble.h"
#include "stm32_seq.h"
#include "shci.h"
#include "usb.h"
#include <bitset>

BleApp bleApp;

PLACE_IN_SECTION("MB_MEM1") ALIGN(4) static TL_CmdPacket_t bleCmdBuffer;


void BleApp::Init()
{
	SHCI_C2_Ble_Init_Cmd_Packet_t ble_init_cmd_packet =	{
			{{0,0,0}},                          // Header unused
			{0,                                 // pBleBufferAddress not used
			0,                                  // BleBufferSize not used
			NumberOfGATTAttributes,
			NumberOfGATTServices,
			CFG_BLE_ATT_VALUE_ARRAY_SIZE,
			CFG_BLE_NUM_LINK,
			DataLengthExtension,
			CFG_BLE_PREPARE_WRITE_LIST_SIZE,
			CFG_BLE_MBLOCK_COUNT,
			CFG_BLE_MAX_ATT_MTU,
			SlaveSleepClockAccuracy,
			MasterSleepClockAccuracy,
			LowSpeedWakeUpClk,
			MaxConnEventLength,
			HseStartupTime,
			ViterbiEnable,
			CFG_BLE_OPTIONS,
			0,									// Hw Version - unused and reserved
			MaxCOChannels,
			MinTransmitPower,
			MaxTransmitPower,
			RXModelConfig,
			MaxAdvertisingSets,
			MaxAdvertisingDataLength,
			TXPathCompensation,
			RXPathCompensation,
			BLECoreVersion,
			CFG_BLE_OPTIONS_EXT
			}
	};

	// Initialize BLE Transport Layer
	HCI_TL_HciInitConf_t Hci_Tl_Init_Conf;
	Hci_Tl_Init_Conf.p_cmdbuffer = (uint8_t*)&bleCmdBuffer;
	Hci_Tl_Init_Conf.StatusNotCallBack = StatusNot;
	hci_init(UserEvtRx, (void*)&Hci_Tl_Init_Conf);

	// Register the hci transport layer to handle BLE User Asynchronous Events
	UTIL_SEQ_RegTask(1 << CFG_TASK_HCI_ASYNCH_EVT_ID, UTIL_SEQ_RFU, hci_user_evt_proc);

	// Start the BLE Stack on CPU2
	if (SHCI_C2_BLE_Init(&ble_init_cmd_packet) != SHCI_Success) {
		coprocessorFailure = true;
		return;
	}

	HciGapGattInit();											// Initialization of HCI & GATT & GAP layer
	SVCCTL_Init();												// Initialization of the BLE Services

	UTIL_SEQ_RegTask(1 << CFG_TASK_ScanRequest, UTIL_SEQ_RFU, ScanRequest);
	UTIL_SEQ_RegTask(1 << CFG_TASK_ConnectRequest, UTIL_SEQ_RFU, ConnectRequest);
	UTIL_SEQ_RegTask(1 << CFG_TASK_DisconnectRequest, UTIL_SEQ_RFU, DisconnectRequest);

	bleApp.connectionStatus = ConnectionStatus::Idle;		// Initialization of the BLE App Context
	aci_hal_set_radio_activity_mask(0x0020);					// Radio mask Activity: Connection event master
	hidApp.Init();												// Initialize HID Client Application
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
			evt_blecore_aci* bleCoreEvent = (evt_blecore_aci*)event_pckt->data;

			switch (bleCoreEvent->ecode)	{

				case ACI_GAP_PROC_COMPLETE_VSEVT_CODE: {
					auto* gapProcComplete = (aci_gap_proc_complete_event_rp0*) bleCoreEvent->data;

					if (gapProcComplete->Procedure_Code == GAP_GENERAL_DISCOVERY_PROC && gapProcComplete->Status == 0x00) {
						printf("* BLE: GAP General discovery procedure completed\r\n");

						// if a device found, connect to it, device 1 being chosen first if both found
						if (action == RequestAction::ScanConnect && deviceServerFound && connectionStatus != BleApp::ConnectionStatus::ClientConnected) {
							UTIL_SEQ_SetTask(1 << CFG_TASK_ConnectRequest, CFG_SCH_PRIO_0);
						}
						action = RequestAction::None;
					}
				}
				break;

				case ACI_L2CAP_CONNECTION_UPDATE_REQ_VSEVT_CODE: {
					auto* connUpdEvent = (aci_l2cap_connection_update_req_event_rp0*) bleCoreEvent->data;
					aci_hal_set_radio_activity_mask(0x0000);

					result = aci_l2cap_connection_parameter_update_resp(connectionHandle,
							connUpdEvent->Interval_Min,
							connUpdEvent->Interval_Max,
							connUpdEvent->Latency,
							connUpdEvent->Timeout_Multiplier,
							CONN_L1,
							CONN_L2,
							connUpdEvent->Identifier,
							0x01);

					aci_hal_set_radio_activity_mask(0x0020);				// 0x0020: Central connection
				}
				break;

				default:
					break;
				}
			}
		break;

		case HCI_DISCONNECTION_COMPLETE_EVT_CODE: {
			auto* disconnectionEvent = (hci_disconnection_complete_event_rp0*) event_pckt->data;
			if (disconnectionEvent->Connection_Handle == connectionHandle) {
				connectionHandle = 0;
				connectionStatus = BleApp::ConnectionStatus::Idle;
				printf("* BLE: Disconnected from device\r\n");
				hidApp.HIDConnectionNotification();
			}
		}
		break;

		case HCI_LE_META_EVT_CODE:
		{
			auto* metaEvent = (evt_le_meta_event*) event_pckt->data;

			switch (metaEvent->subevent)
			{
				case HCI_LE_CONNECTION_COMPLETE_SUBEVT_CODE:
				{
					// The connection is done
					auto* connectionCompleteEvent = (hci_le_connection_complete_event_rp0*) metaEvent->data;
					connectionHandle = connectionCompleteEvent->Connection_Handle;
					connectionStatus = BleApp::ConnectionStatus::ClientConnected;

					// Notify HidApp
					printf("* BLE: Connected to server\r\n");
					hidApp.HIDConnectionNotification();

					if (action == RequestAction::GetReportMap) {
						hidApp.action = HidApp::HidAction::GetReportMap;
					} else {
						hidApp.action = HidApp::HidAction::Connect;
					}
					action = RequestAction::None;

					result = aci_gatt_disc_all_primary_services(connectionHandle);

					if (result == BLE_STATUS_SUCCESS) {
						printf("* BLE: Start Searching Primary Services\r\n");
					} else {
						printf("* BLE: All services discovery Failed\r\n");
					}

					break;
				}

				case HCI_LE_ADVERTISING_REPORT_SUBEVT_CODE:
				{
					auto* advertisingEvent = (hci_le_advertising_report_event_rp0*) metaEvent->data;
					uint8_t event_type = advertisingEvent->Advertising_Report[0].Event_Type;
					uint8_t event_data_size = advertisingEvent->Advertising_Report[0].Length_Data;

					std::unique_ptr<AdvertisingReport> ar = std::make_unique<AdvertisingReport>();

					memcpy(ar->address, advertisingEvent->Advertising_Report[0].Address, bleAddrSize);

					// When decoding advertising report the data and RSSI values must be read by using offsets
					// RSSI = (int8_t)*(uint8_t*) (adv_report_data + advertisingEvent->Advertising_Report[0].Length_Data);
					uint8_t* adv_report_data = (uint8_t*)(&advertisingEvent->Advertising_Report[0].Length_Data) + 1;
					int i = 0;

					// search AD Type for HID Devices
					if (event_type == ADV_IND) {

						while (i < event_data_size) {
							uint8_t adlength = adv_report_data[i];
							uint8_t adtype = adv_report_data[i + 1];
							switch (adtype)
							{
								case AD_TYPE_FLAGS:
									ar->flags = adv_report_data[i + 2];
									break;

								case AD_TYPE_16_BIT_SERV_UUID_CMPLT_LIST:
									ar->serviceClasses = *(uint16_t*)&adv_report_data[i + 2];
									if (action == RequestAction::ScanConnect && ar->serviceClasses == 0x1812) {	// FIXME whitelist etc
										printf("* BLE: Server detected - HID Device\r\n");
										deviceServerFound = true;
										memcpy(&deviceAddress, advertisingEvent->Advertising_Report[0].Address, bleAddrSize);
										deviceAddressType = (GapAddress)advertisingEvent->Advertising_Report[0].Address_Type;

										// If connecting terminate scan - will fire ACI_GAP_PROC_COMPLETE_VSEVT_CODE event which will initiate connection
										aci_gap_terminate_gap_proc(GAP_GENERAL_DISCOVERY_PROC);
									}
									break;

								case AD_TYPE_SHORTENED_LOCAL_NAME:
									ar->shortName = std::string ((char*)&adv_report_data[i + 2], adlength - 1);
									break;

								case AD_TYPE_COMPLETE_LOCAL_NAME:
									ar->shortName = std::string ((char*)&adv_report_data[i + 2], adlength - 1);
									break;

								case AD_TYPE_APPEARANCE:
									ar->appearance = *(uint16_t*)&adv_report_data[i + 2];
									break;

								case AD_TYPE_MANUFACTURER_SPECIFIC_DATA:
									ar->manufactLen = std::min((unsigned int)adlength, sizeof ar->manufactData);
									memcpy(ar->manufactData, &adv_report_data[i + 2], ar->manufactLen);
									break;

								default:
									break;
							}
							i += adlength + 1;
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
	usb.cdc.PrintString("* BLE Found device:"
			"\r\n  Address: %02X%02X%02X%02X%02X%02X"
			"\r\n  Flags: %s"
			"\r\n  Name: %s"
			"\r\n  Appearance: %02X"
			"\r\n  Service class: %02X"
			"\r\n  Manufacturer data: %s"
			"\r\n\r\n",
			ar->address[5], ar->address[4], ar->address[3], ar->address[2], ar->address[1], ar->address[0],
			std::bitset<8>(ar->flags).to_string().c_str(),
			ar->shortName.c_str(),
			ar->appearance,
			ar->serviceClasses,
			usb.cdc.HexToString(ar->manufactData, ar->manufactLen, true)
	);
}


BleApp::ConnectionStatus BleApp::GetClientConnectionStatus(uint16_t connHandle)
{
	if (connectionHandle == connHandle) {
		return connectionStatus;
	}
	return ConnectionStatus::Idle;
}


void BleApp::ScanInfo()
{
	if (bleApp.connectionStatus == ConnectionStatus::Idle) {
		if (bleApp.action == RequestAction::ScanInfo) {
			aci_gap_terminate_gap_proc(GAP_GENERAL_DISCOVERY_PROC);			// If currently scanning terminate
			printf("Still scanning ...\n");
			return;
		}
		bleApp.action = RequestAction::ScanInfo;
		UTIL_SEQ_SetTask(1 << CFG_TASK_ScanRequest, CFG_SCH_PRIO_0);
	} else {
		printf("Connection is busy ...\n");
	}
}


void BleApp::GetHidReportMap(uint8_t* address)			// FIXME: need to handle random/public address type
{
	if (bleApp.connectionStatus == ConnectionStatus::Idle) {
		bleApp.action = RequestAction::GetReportMap;
		memcpy(&deviceAddress, address, bleAddrSize);
		UTIL_SEQ_SetTask(1 << CFG_TASK_ConnectRequest, CFG_SCH_PRIO_0);
	} else {
		printf("Connection is busy ...\n");
	}
}


void BleApp::SwitchConnectState()
{
	// Called from connect button interrupt (or CDC console)
	if (hidApp.state != HidApp::HidState::ClientConnected)	{
		bleApp.action = RequestAction::ScanConnect;
		UTIL_SEQ_SetTask(1 << CFG_TASK_ScanRequest, CFG_SCH_PRIO_0);
	} else {
		UTIL_SEQ_SetTask(1 << CFG_TASK_DisconnectRequest, CFG_SCH_PRIO_0);
	}
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
	aci_hal_set_tx_power_level(1, TransmitPower);

	// Initialize GATT interface
	aci_gatt_init();

	// Initialize GAP interface
	aci_gap_init(GAP_CENTRAL_ROLE, 0, strlen(deviceName), &gap_service_handle, &gap_dev_name_char_handle, &gap_appearance_char_handle);

	if (aci_gatt_update_char_value(gap_service_handle, gap_dev_name_char_handle, 0, strlen(deviceName), (uint8_t*)deviceName)) {
		printf("Device Name aci_gatt_update_char_value failed.\n");
	}

	if (aci_gatt_update_char_value(gap_service_handle, gap_appearance_char_handle, 0, 2, (uint8_t*)&appearance)) {
		printf("Appearance aci_gatt_update_char_value failed.\n");
	}

	// Initialize IO capability
	aci_gap_set_io_capability(Security.ioCapability);

	// Initialize authentication
	aci_gap_set_authentication_requirement(
			Security.bondingMode,
			Security.mitmMode,
			Security.secureSupport,
			Security.keypressNotificationSupport,
			Security.encryptionKeySizeMin,
			Security.encryptionKeySizeMax,
			Security.useFixedPin,
			Security.fixedPin,
			Security.BLEAddressType
			);

	// Initialize whitelist
	if (Security.bondingMode) {
		aci_gap_configure_whitelist();
	}
}


void BleApp::ScanRequest()
{
	if (bleApp.connectionStatus != ConnectionStatus::ClientConnected) {
		bleApp.deviceServerFound = false;
		tBleStatus result = aci_gap_start_general_discovery_proc(SCAN_P, SCAN_L, GAP_PUBLIC_ADDR, 1);
		if (result == BLE_STATUS_SUCCESS) {
			printf("* BLE: Start general discovery\r\n");
		} else {
			printf("* BLE: aci_gap_start_general_discovery_proc, Failed \r\n");
		}
	}
}


void BleApp::ConnectRequest()
{
	printf("* BLE: Create connection to server\r\n");

	if (bleApp.connectionStatus != ConnectionStatus::ClientConnected) {
		tBleStatus result = aci_gap_create_connection(SCAN_P,
				SCAN_L,
				(uint8_t)bleApp.deviceAddressType,				// Peer address type & address
				bleApp.deviceAddress,
				(uint8_t)bleApp.Security.BLEAddressType,		// Own address type
				CONN_P1,
				CONN_P2,
				0,
				SUPERV_TIMEOUT,
				CONN_L1,
				CONN_L2);

		if (result == BLE_STATUS_SUCCESS) {
			bleApp.connectionStatus = ConnectionStatus::Connecting;
		} else {
			bleApp.connectionStatus = ConnectionStatus::Idle;
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


