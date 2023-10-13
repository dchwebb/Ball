#include "initialisation.h"
#include "BleApp.h"
#include "ble.h"
#include "BasService.h"
#include "DisService.h"
#include "HidService.h"
#include "stm32_seq.h"
#include "shci.h"
#include "usb.h"
#include "gyroSPI.h"
#include "led.h"

BleApp bleApp;


PLACE_IN_SECTION("MB_MEM1") ALIGN(4) static TL_CmdPacket_t bleCmdBuffer;

// These are the two tags used to manage a power failure during OTA
// The MagicKeywordAdress shall be mapped @0x140 from start of the binary image
// The MagicKeywordvalue is checked in the ble_ota application
PLACE_IN_SECTION("TAG_OTA_END") const uint32_t MagicKeywordValue = 0x94448A29;
PLACE_IN_SECTION("TAG_OTA_START") const uint32_t MagicKeywordAddress = (uint32_t)&MagicKeywordValue;


void BleApp::Init()
{
	// Initialize BLE Transport Layer
	HCI_TL_HciInitConf_t Hci_Tl_Init_Conf;
	Hci_Tl_Init_Conf.p_cmdbuffer = (uint8_t*)&bleCmdBuffer;
	Hci_Tl_Init_Conf.StatusNotCallBack = StatusNotify;
	hci_init(UserEvtRx, (void*) &Hci_Tl_Init_Conf);

	// Register the hci transport layer to handle BLE User Asynchronous Events
	UTIL_SEQ_RegTask(1 << CFG_TASK_HCI_ASYNCH_EVT_ID, UTIL_SEQ_RFU, hci_user_evt_proc);


	SHCI_C2_Ble_Init_Cmd_Packet_t initCmdPacket = {
			{{0,0,0}},                          // Header unused
			{0,                                 // pBleBufferAddress not used
			0,                         			// BleBufferSize not used
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
			CFG_BLE_OPTIONS_EXT}
	};

	auto result = SHCI_C2_BLE_Init(&initCmdPacket);		// Start the BLE Stack on CPU2
	if (result != SHCI_Success) {
		coprocessorFailure = true;
		return;
	}

	HciGapGattInit();							// Initialization of HCI & GATT & GAP layer

	UTIL_SEQ_RegTask(1 << CFG_TASK_SwitchLPAdvertising, UTIL_SEQ_RFU, SwitchLPAdvertising);
	UTIL_SEQ_RegTask(1 << CFG_TASK_CancelAdvertising, UTIL_SEQ_RFU, CancelAdvertising);
	UTIL_SEQ_RegTask(1 << CFG_TASK_GoToSleep, UTIL_SEQ_RFU, GoToSleep);
	UTIL_SEQ_RegTask(1 << CFG_TASK_EnterSleepMode, UTIL_SEQ_RFU, EnterSleepMode);

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
			printf("\r\n\r** Client Disconnected\n");

			EnableAdvertising(ConnStatus::FastAdv);			// Restart advertising
		}

		break;

		case HCI_LE_META_EVT_CODE:
		{
			auto meta_evt = (evt_le_meta_event*) event_pckt->data;

			if (meta_evt->subevent == HCI_LE_CONNECTION_COMPLETE_SUBEVT_CODE) {
				auto connCompleteEvent = (hci_le_connection_complete_event_rp0*) meta_evt->data;

				RTCInterrupt(0);								// Disable low power advertising timer

				printf("Client connected: handle 0x%x\n", connCompleteEvent->Connection_Handle);
				connectionStatus = ConnStatus::Connected;
				connectionHandle = connCompleteEvent->Connection_Handle;
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
					printf("\r\n\r** ACI_GAP_PROC_COMPLETE_VSEVT_CODE \n");
					break;

				case ACI_HAL_END_OF_RADIO_ACTIVITY_VSEVT_CODE:
					break;

				case (ACI_GAP_KEYPRESS_NOTIFICATION_VSEVT_CODE):
					printf("\r\n\r** ACI_GAP_KEYPRESS_NOTIFICATION_VSEVT_CODE \n");
					break;

				case ACI_GAP_PASS_KEY_REQ_VSEVT_CODE:
					aci_gap_pass_key_resp(connectionHandle, Security.fixedPin);
					break;

				case ACI_GAP_NUMERIC_COMPARISON_VALUE_VSEVT_CODE: {
					auto evt_numeric_value = (aci_gap_numeric_comparison_value_event_rp0*)blecore_evt->data;
					uint32_t numeric_value = evt_numeric_value->Numeric_Value;
					printf("numeric_value = %ld\n", numeric_value);
					aci_gap_numeric_comparison_value_confirm_yesno(connectionHandle, true);
					break;
				}

				case ACI_GAP_PAIRING_COMPLETE_VSEVT_CODE: {
					auto pairing_complete = (aci_gap_pairing_complete_event_rp0*)blecore_evt->data;
					printf("ACI_GAP_PAIRING_COMPLETE_VSEVT_CODE: Status = %d\n", pairing_complete->Status);
					break;
				}
			}
			break;
		}

		default:
			break;
	}

}


void BleApp::HciGapGattInit()
{
	uint16_t gap_service_handle, gap_dev_name_char_handle, gap_appearance_char_handle;

	uint16_t appearance[1] = { BLE_CFG_GAP_APPEARANCE };

	hci_reset();									// HCI Reset to synchronise BLE Stack

	// Write the BD Address
	aci_hal_write_config_data(CONFIG_DATA_PUBADDR_OFFSET, bdAddrSize, GetBdAddress());

	// Write root keys used to derive LTK and CSRK
	aci_hal_write_config_data(CONFIG_DATA_IR_OFFSET, CONFIG_DATA_IR_LEN, IdentityRootKey);
	aci_hal_write_config_data(CONFIG_DATA_ER_OFFSET, CONFIG_DATA_ER_LEN, EncryptionRootKey);

	aci_hal_set_tx_power_level(0, settings.transmitPower);	// Set TX Power to 0dBm.
	aci_gatt_init();								// Initialize GATT interface

	// Initialize GAP interface
	aci_gap_init(
			GAP_PERIPHERAL_ROLE,
			0,
			GapDeviceName.length(),
			&gap_service_handle,
			&gap_dev_name_char_handle,
			&gap_appearance_char_handle
			);

	if (aci_gatt_update_char_value(
			gap_service_handle,
			gap_dev_name_char_handle,
			0,
			GapDeviceName.length(),
			(uint8_t*)std::string(GapDeviceName).c_str())
		) {
		printf("Device Name aci_gatt_update_char_value failed.\n");
	}

	if (aci_gatt_update_char_value(gap_service_handle, gap_appearance_char_handle, 0, 2, (uint8_t*)&appearance)) {
		printf("Appearance aci_gatt_update_char_value failed.\n");
	}

	// Initialize Default PHY - specify preference for 1 or 2Mbit Phy for TX and RX
	hci_le_set_default_phy(PreferAllPhy, Prefer2MbitPhy, Prefer2MbitPhy);

	// Initialize IO capability
	aci_gap_set_io_capability(Security.ioCapability);

	// Initialize authentication
	aci_gap_set_authentication_requirement(Security.bondingMode,
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


void BleApp::EnableAdvertising(const ConnStatus newStatus)
{
	tBleStatus ret = BLE_STATUS_INVALID_PARAMS;

	// If switching to LP advertising first disable fast advertising
	if (newStatus == ConnStatus::LPAdv && connectionStatus == ConnStatus::FastAdv) {
		ret = aci_gap_set_non_discoverable();
		if (ret) {
			printf("Stop Advertising Failed , result: %d \n", ret);
		}
	}
	connectionStatus = newStatus;

	const uint16_t minInterval = (newStatus == ConnStatus::FastAdv) ? FastAdvIntervalMin : LPAdvIntervalMin;
	const uint16_t maxInterval = (newStatus == ConnStatus::FastAdv) ? FastAdvIntervalMax : LPAdvIntervalMax;
	ret = aci_gap_set_discoverable((uint8_t)AdvertisingType::Indirect, minInterval, maxInterval, Security.BLEAddressType, HCI_ADV_FILTER_NO, 0, 0, 0, 0, 0, 0);
	ret = aci_gap_update_adv_data(sizeof(ad_data), ad_data);		// Update Advertising data

	if (ret == BLE_STATUS_SUCCESS) {
		if (newStatus == ConnStatus::FastAdv)	{
			printf("Start Fast Advertising\r\n");
			wakeAction = WakeAction::LPAdvertising;					// Set action when waking up
			RTCInterrupt(settings.fastAdvTimeout);					// Trigger RTC interrupt to start low speed advertising
		} else {
			printf("Switch to Low Power Advertising\r\n");
			wakeAction = WakeAction::Shutdown;						// Set ato shutdown if not connected after LP adv period
			RTCInterrupt(settings.lpAdvTimeout);
		}
	} else {
		printf("Start Advertising Failed , result: %d \n", ret);
	}

}


uint8_t* BleApp::GetBdAddress()
{
	const uint32_t udn = LL_FLASH_GetUDN();
	const uint32_t companyId = LL_FLASH_GetSTCompanyID();
	const uint32_t deviceId = LL_FLASH_GetDeviceID();

	// Public Address with the ST company ID
	// bit[47:24] : 24 bits (OUI) equal to the company ID
	// bit[23:16] : Device ID.
	// bit[15:0]  : The last 16 bits from the UDN
	// Note: In order to use the Public Address in a final product, a dedicated 24 bit company ID (OUI) must be bought.
	bdAddrUdn[0] = (uint8_t)(udn & 0x000000FF);
	bdAddrUdn[1] = (uint8_t)((udn & 0x0000FF00) >> 8);
	bdAddrUdn[2] = (uint8_t)deviceId;
	bdAddrUdn[3] = (uint8_t)(companyId & 0x000000FF);
	bdAddrUdn[4] = (uint8_t)((companyId & 0x0000FF00) >> 8);
	bdAddrUdn[5] = (uint8_t)((companyId & 0x00FF0000) >> 16);

	return bdAddrUdn;
}


void BleApp::SwitchLPAdvertising()
{
	bleApp.EnableAdvertising(ConnStatus::LPAdv);
}


void BleApp::CancelAdvertising()
{
	// Used to initiate sleep process
	if (bleApp.connectionStatus == ConnStatus::FastAdv || bleApp.connectionStatus == ConnStatus::LPAdv) {
		bleApp.connectionStatus = ConnStatus::Idle;

		tBleStatus result = aci_gap_set_non_discoverable();
		if (result == BLE_STATUS_SUCCESS) {
			printf("Stop advertising\r\n");
		} else {
			printf("Stop advertising Failed\r\n");
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


void BleApp::InactivityTimeout()
{
	// When connected and no movement for a short period go to sleep allowing wake up from gyroscope interrupt
	// Before sleeping, set a longer RTC wakeup interrupt to shutdown system if no movement
	lowPowerMode = LowPowerMode::Sleep;
	sleepState = SleepState::RequestSleep;
	wakeAction = WakeAction::Shutdown;							// Shutdown if movement for significant period

	// If not waking because motion detected, the shutdown timer will be already set
	if ((RTC->CR & RTC_CR_WUTE) == 0) {
		RTCInterrupt(settings.inactiveTimeout);					// Trigger RTC to wake up from sleep and shutdown after timeout
	}
}


void BleApp::RTCWakeUp()
{
	// Manage RTC timer interrupts
	RTCInterrupt(0);						// Clear RTC wakeup interrupt

	switch (wakeAction) {
	case WakeAction::LPAdvertising:
		UTIL_SEQ_SetTask(1 << CFG_TASK_SwitchLPAdvertising, CFG_SCH_PRIO_0);
		break;
	case WakeAction::Shutdown:
		// RTC interrupt fires to wake up after long sleep inactivity: WakeFromSleep will then immediately trigger shutdown
		bleApp.sleepState = BleApp::SleepState::WakeToShutdown;
		break;
	}
}


void BleApp::GoToSleep()
{
	if (bleApp.connectionStatus == ConnStatus::Connected) {
		bleApp.sleepState = SleepState::GoToSleep;
		UTIL_SEQ_SetTask(1 << CFG_TASK_EnterSleepMode, CFG_SCH_PRIO_0);
	} else {
		RTCInterrupt(0);					// Cancel advertising timers (if connected interrupt will be used for inactivity shutdown)
		bleApp.sleepState = SleepState::CancelAdv;
		UTIL_SEQ_SetTask(1 << CFG_TASK_CancelAdvertising, CFG_SCH_PRIO_0);
	}
}


void BleApp::EnterSleepMode()
{
	const uint32_t primask_bit = __get_PRIMASK();

	led.Update();														// Force LED to switch off
	bleApp.motionWakeup = false;										// Will be set if gyro motion wakes from sleep

	__disable_irq();													// Disable interrupts
	SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;							// Disable Systick interrupt

	if (bleApp.lowPowerMode == LowPowerMode::Sleep) {
		gyro.Configure(GyroSPI::SetConfig::WakeupInterrupt);			// Enable Gyroscope Interrupt output pin for wakeup
	} else {
		gyro.Configure(GyroSPI::SetConfig::PowerDown);					// Power down Gyroscope
		usb.Disable();
	}

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

	if (bleApp.lowPowerMode == LowPowerMode::Shutdown){
		RCC->CR &= ~RCC_CR_HSEON;										// Turn off external oscillator
		RCC->BDCR &= ~RCC_BDCR_LSEON;									// Turn off low speed external oscillator

		SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;								// SLEEPDEEP must be set for STOP, STANDBY or SHUTDOWN modes

		MODIFY_REG(PWR->CR1, PWR_CR1_LPMS, LL_PWR_MODE_SHUTDOWN);
		MODIFY_REG(PWR->C2CR1, PWR_C2CR1_LPMS, LL_PWR_MODE_SHUTDOWN);

		EXTI->C2IMR2 = 0;												// Clear all wake up interrupts on CPU2
	}


	PWR->SCR |= PWR_SCR_CWUF;											// Clear all wake up flags
	__set_PRIMASK(primask_bit);											// Re-enable interrupts for exiting sleep mode
	__WFI();															// Activates sleep (wait for interrupts)

	bleApp.WakeFromSleep();
}


void BleApp::WakeFromSleep()
{
	if (sleepState == SleepState::WakeToShutdown) {						// If no activity during sleep, RTC interrupt will wake up to shutdown
		lowPowerMode = LowPowerMode::Shutdown;
		EnterSleepMode();
		return;
	}

	while (LL_HSEM_1StepLock(HSEM, CFG_HW_RCC_SEMID));					// Lock the RCC semaphore
	MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW);					// 10: PLL selected as system clock
	while ((RCC->CFGR & RCC_CFGR_SWS) == 0);							// Wait until HSE is selected
	LL_HSEM_ReleaseLock(HSEM, CFG_HW_RCC_SEMID, 0);						// Release RCC semaphore

	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;							// Restart Systick interrupt

	if (connectionStatus == ConnStatus::Connected) {
		if (bleApp.motionWakeup) {						// Set in gyro motion interrupt		 || bleApp.settings.sleepUsesHSI
			RTCInterrupt(0);											// Cancel shutdown timeout only if waking up with movement
			gyro.Configure(GyroSPI::SetConfig::ContinousOutput);		// Turn Gyroscope back on
		} else {
			sleepState = SleepState::RequestSleep;
			return;
		}
	} else {
		gyro.Configure(GyroSPI::SetConfig::PowerDown);					// Power down Gyroscope
		EnableAdvertising(ConnStatus::FastAdv);
	}

	sleepState = SleepState::Awake;
}


void BleApp::UserEvtRx(void* pPayload)
{
	// This will first call the service handlers (hids, bas etc) which try and match on Attribute Handles.
	// If not handled will then call bleApp.ServiceControlCallback() for connection/security handling

	auto pParam = (tHCI_UserEvtRxParam*)pPayload;
	auto event_pckt = (hci_event_pckt*)&(pParam->pckt->evtserial.evt);

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


uint32_t BleApp::SerialiseConfig(uint8_t** buff)
{
	*buff = reinterpret_cast<uint8_t*>(&settings);
	return sizeof(settings);
}


uint32_t BleApp::StoreConfig(uint8_t* buff)
{
	if (buff != nullptr) {
		memcpy(&settings, buff, sizeof(settings));
	}

	return sizeof(settings);
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


void SVCCTL_ResumeUserEventFlow(void)
{
	hci_resume_flow();
}

