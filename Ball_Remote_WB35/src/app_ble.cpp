#include "main.h"
#include "ble.h"
#include "app_ble.h"
#include "stm32_seq.h"
#include "shci.h"


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

	TransportLayerInit();						// Initialize Ble Transport Layer

	// Register the hci transport layer to handle BLE User Asynchronous Events
	UTIL_SEQ_RegTask(1 << CFG_TASK_HCI_ASYNCH_EVT_ID, UTIL_SEQ_RFU, hci_user_evt_proc);

	auto result = SHCI_C2_BLE_Init(&ble_init_cmd_packet);		// Starts the BLE Stack on CPU2
	if (result != SHCI_Success) {
		coprocessorFailure = true;
		return;
	}

	HciGapGattInit();							// Initialization of HCI & GATT & GAP layer

	UTIL_SEQ_RegTask(1 << CFG_TASK_SwitchLPAdvertising, UTIL_SEQ_RFU, SwitchLPAdvertising);
	UTIL_SEQ_RegTask(1 << CFG_TASK_SwitchFastAdvertising, UTIL_SEQ_RFU, SwitchFastAdvertising);
	UTIL_SEQ_RegTask(1 << CFG_TASK_CancelAdvertising, UTIL_SEQ_RFU, CancelAdvertising);
	UTIL_SEQ_RegTask(1 << CFG_TASK_GoToSleep, UTIL_SEQ_RFU, GoToSleep);
	UTIL_SEQ_RegTask(1 << CFG_TASK_EnterSleepMode, UTIL_SEQ_RFU, EnterSleepMode);

	// Create timer to switch to Low Power advertising
	HW_TS_Create(CFG_TIM_PROC_ID_ISR, &(lowPowerAdvTimerId), hw_ts_SingleShot, QueueLPAdvertising);

	disService.Init();							// Initialize Device Information Service
	basService.Init();							// Initialize Battery level Service
	hidService.Init();							// Initialize HID Service

	EnableAdvertising(ConnStatus::FastAdv);		// Start advertising for client connections
}


void BleApp::ServiceControlCallback(hci_event_pckt* event_pckt)
{

	switch (event_pckt->evt) {
		case HCI_DISCONNECTION_COMPLETE_EVT_CODE:
		{
			[[maybe_unused]] auto disconnection_complete_event = (hci_disconnection_complete_event_rp0*) event_pckt->data;

			connectionHandle = 0xFFFF;
			connectionStatus = ConnStatus::Idle;
			hidService.Disconnect();

			APP_DBG_MSG("\r\n\r** Disconnection event with client \n");

			EnableAdvertising(ConnStatus::FastAdv);			// restart advertising

			HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, GPIO_PIN_RESET);
		}

		break;

		case HCI_LE_META_EVT_CODE:
		{
			auto meta_evt = (evt_le_meta_event*) event_pckt->data;

			if (meta_evt->subevent == HCI_LE_CONNECTION_COMPLETE_SUBEVT_CODE) {
				auto connCompleteEvent = (hci_le_connection_complete_event_rp0*) meta_evt->data;

				HW_TS_Stop(lowPowerAdvTimerId);				// connected: no need anymore to schedule the LP ADV

				APP_DBG_MSG("Client connected: handle 0x%x\n", connCompleteEvent->Connection_Handle);
				connectionStatus = ConnStatus::Connected;
				connectionHandle = connCompleteEvent->Connection_Handle;

				HAL_GPIO_WritePin(RED_LED_GPIO_Port, RED_LED_Pin, GPIO_PIN_SET);
				HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_RESET);		// Turn off advertising LED
			}
		}
		break;

		case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
		{
			auto blecore_evt = (evt_blecore_aci*) event_pckt->data;

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

				case ACI_GAP_NUMERIC_COMPARISON_VALUE_VSEVT_CODE: {
					auto evt_numeric_value = (aci_gap_numeric_comparison_value_event_rp0*)blecore_evt->data;
					uint32_t numeric_value = evt_numeric_value->Numeric_Value;
					APP_DBG_MSG("numeric_value = %ld\n", numeric_value);
					aci_gap_numeric_comparison_value_confirm_yesno(connectionHandle, YES);
					break;
				}

				case ACI_GAP_PAIRING_COMPLETE_VSEVT_CODE: {
					auto pairing_complete = (aci_gap_pairing_complete_event_rp0*)blecore_evt->data;
					APP_DBG_MSG("ACI_GAP_PAIRING_COMPLETE_VSEVT_CODE: Status = %d\n", pairing_complete->Status);
					break;
				}
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
	Hci_Tl_Init_Conf.StatusNotCallBack = StatusNotify;
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
		APP_DBG_MSG("Device Name aci_gatt_update_char_value failed.\n");
	}

	if (aci_gatt_update_char_value(gap_service_handle, gap_appearance_char_handle, 0, 2, (uint8_t*)&appearance)) {
		APP_DBG_MSG("Appearance aci_gatt_update_char_value failed.\n");
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
			Security.useFixedPin,
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
	// Queues transition from fast to low power advertising
	UTIL_SEQ_SetTask(1 << CFG_TASK_SwitchLPAdvertising, CFG_SCH_PRIO_0);
}


void BleApp::QueueFastAdvertising()
{
	// Queues activation of fast advertising
	UTIL_SEQ_SetTask(1 << CFG_TASK_SwitchFastAdvertising, CFG_SCH_PRIO_0);
}


void BleApp::SwitchLPAdvertising()
{
	bleApp.EnableAdvertising(ConnStatus::LPAdv);
}


void BleApp::SwitchFastAdvertising()
{
	bleApp.EnableAdvertising(ConnStatus::FastAdv);
}

void BleApp::CancelAdvertising()
{
	if (bleApp.connectionStatus == ConnStatus::FastAdv || bleApp.connectionStatus == ConnStatus::LPAdv) {
		HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_RESET);
		HW_TS_Stop(bleApp.lowPowerAdvTimerId);					// Cancel timer to activate LP Advertising

		tBleStatus result = aci_gap_set_non_discoverable();

		bleApp.connectionStatus = ConnStatus::Idle;
		if (result == BLE_STATUS_SUCCESS) {
			APP_DBG_MSG("Stop advertising \r\n");
		} else {
			APP_DBG_MSG("Stop advertising Failed \r\n");
		}
	}

	// As advertising must be stopped before entering sleep mode, trigger task here
	if (bleApp.sleepState == SleepState::CancelAdv) {
		bleApp.sleepState = SleepState::GoToSleep;
		UTIL_SEQ_SetTask(1 << CFG_TASK_EnterSleepMode, CFG_SCH_PRIO_0);
	}
}


void BleApp::DisconnectRequest()
{
	aci_gap_terminate(bleApp.connectionHandle, 0x16);			// 0x16: Connection Terminated by Local Host
}


void BleApp::GoToSleep()
{
	bleApp.sleepState = SleepState::CancelAdv;
	UTIL_SEQ_SetTask(1 << CFG_TASK_CancelAdvertising, CFG_SCH_PRIO_0);
}


static void SwitchToHSI()
{
	MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, LL_RCC_SYS_CLKSOURCE_HSI);			// Set HSI as clock source
	while ((RCC->CFGR & RCC_CFGR_SWS) != LL_RCC_SYS_CLKSOURCE_STATUS_HSI);	// Wait until HSE is selected
}


void BleApp::EnterSleepMode()
{
	HAL_GPIO_WritePin(BLUE_LED_GPIO_Port, BLUE_LED_Pin, GPIO_PIN_RESET);

	uint32_t primask_bit = __get_PRIMASK();
	__disable_irq();													// Disable interrupts

	SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;							// Disable Systick interrupt

	while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID));					// Lock the RCC semaphore

	if (!LL_HSEM_1StepLock(HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID)) {		// Lock the Stop mode entry semaphore
		if (LL_PWR_IsActiveFlag_C2DS() || LL_PWR_IsActiveFlag_C2SB()) {	// PWR->EXTSCR: C2DS = CPU2 in deep sleep; C2SBF = System has been in Shutdown mode
			LL_HSEM_ReleaseLock(HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID, 0);	// Release ENTRY_STOP_MODE semaphore
			SwitchToHSI();
		}
	} else {
		SwitchToHSI();
	}

	LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, 0);						// Release RCC semaphore

	// See p153 for LP modes entry and wake up
	RCC->CFGR |= RCC_CFGR_STOPWUCK;										// HSI16 selected as wakeup from stop clock and CSS backup clock
	SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;									// SLEEPDEEP must be set for STOP, STANDBY or SHUTDOWN modes

	if (bleApp.lowPowerMode == LowPowerMode::Stop) {
		MODIFY_REG(PWR->CR1, PWR_CR1_LPMS, LL_PWR_MODE_STOP1);			// CPU1: 000: Stop0 mode, 001: Stop1 mode, 010: Stop2 mode, 011: Standby mode, 1xx: Shutdown mode
		//MODIFY_REG(PWR->C2CR1, PWR_C2CR1_LPMS, LL_PWR_MODE_STANDBY);	// CPU2 low power mode: note this doesn't do anything in most cases as CPU2
	} else {
		MODIFY_REG(PWR->CR1, PWR_CR1_LPMS, LL_PWR_MODE_SHUTDOWN);
		MODIFY_REG(PWR->C2CR1, PWR_C2CR1_LPMS, LL_PWR_MODE_SHUTDOWN);

		EXTI->C2IMR2 = 0;												// Clear all wake up interrupts on CPU2
		for (int x = 0; x < 1000000; ++x) {								// Force a delay as otherwise something was using 4mA
			__attribute__((unused)) volatile int y = 1;
		}
	}

	PWR->SCR |= PWR_SCR_CWUF;											// Clear all wake up flags
	__set_PRIMASK(primask_bit);											// Re-enable interrupts for exiting sleep mode
	__WFI();															// Activates sleep (wait for interrupts)

	bleApp.WakeFromSleep();
}


void BleApp::WakeFromSleep()
{
	// Executes on wake-up
	RCC->CR |= RCC_CR_HSEON;							// Turn on external oscillator
	while ((RCC->CR & RCC_CR_HSERDY) == 0);				// Wait till HSE is ready
	MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, 0b10);			// 10: HSE selected as system clock
	while ((RCC->CFGR & RCC_CFGR_SWS) == 0);			// Wait until HSE is selected

	SystemCoreClockUpdate();							// Read configured clock speed into SystemCoreClock (system clock frequency)
	sleepState = SleepState::Awake;

	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;			// Restart Systick interrupt

	APP_DBG_MSG("\nWaking up\n");
	EnableAdvertising(ConnStatus::FastAdv);
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


void BleApp::UserEvtRx(void* pPayload)
{
	// This will first call the service handlers (hids, bas etc) which try and match on Attribute Handles.
	// If not handled will then call bleApp.ServiceControlCallback() for connection/security handling

	auto pParam = (tHCI_UserEvtRxParam*)pPayload;
	auto event_pckt = (hci_event_pckt *)&(pParam->pckt->evtserial.evt);

	if (event_pckt->evt == HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE) {
		if (hidService.EventHandler(event_pckt)) {
			return;
		}
		if (basService.EventHandler(event_pckt)) {
			return;
		}
	}

	bleApp.ServiceControlCallback(event_pckt);
}


// This callback is triggered at the start and end of hci_send_req() to halt all pending tasks and then resume when command complete
void BleApp::StatusNotify(HCI_TL_CmdStatus_t status)
{
	uint32_t task_id_list = (1 << CFG_LAST_TASK_ID_WITH_HCICMD) - 1;
	switch (status)	{
	case HCI_TL_CmdBusy:
		UTIL_SEQ_PauseTask(task_id_list);
		break;

	case HCI_TL_CmdAvailable:
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

