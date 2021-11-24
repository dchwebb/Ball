//#include "app_common.h"
#include "main.h"
#include "app_entry.h"
#include "app_ble.h"
//#include "ble.h"
//#include "tl.h"
#include "stm32_seq.h"
#include "shci_tl.h"
#include "stm32_lpm.h"
#include "app_debug.h"
#include "otp.h"

extern RTC_HandleTypeDef hrtc;

#define POOL_SIZE (CFG_TLBLE_EVT_QUEUE_LENGTH * 4U * DIVC((sizeof(TL_PacketHeader_t) + TL_BLE_EVENT_FRAME_SIZE), 4U))

PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t EvtPool[POOL_SIZE];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static TL_CmdPacket_t SystemCmdBuffer;
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t SystemSpareEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255U];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t BleSpareEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255];

static void Config_HSE(void);
static void Reset_IPCC();
static void Reset_BackupDomain();
static void SystemPower_Config();
static void appe_Tl_Init();
static void APPE_SysStatusNot(SHCI_TL_CmdStatus_t status);
static void APPE_SysUserEvtRx(void* pPayload);
static void Init_Rtc();
static void Init_Exti();


void MX_APPE_Config()
{
	// The OPTVERR flag is wrongly set at power on. It shall be cleared before using any HAL_FLASH_xxx() api
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

	// Reset some configurations so that the system behave in the same way when either out of nReset or Power On
	Reset_BackupDomain();
	Reset_IPCC();

	// Configure HSE Tuning
	Config_HSE();
}

void MX_APPE_Init()
{
	Init_Exti();
	Init_Rtc();
	SystemPower_Config();
	HW_TS_Init(hw_ts_InitMode_Full, &hrtc); 	// Initialize the TimerServer
	appe_Tl_Init();								// Initialize all transport layers

	// From now, the application is waiting for the ready event (VS_HCI_C2_Ready) received on the system channel before starting the Stack
	// This system event is received with APPE_SysUserEvtRx()
}


void Init_Exti()
{
	// Disable all wakeup interrupt on CPU1  except IPCC(36), HSEM(38)
	LL_EXTI_DisableIT_0_31(~0);
	LL_EXTI_DisableIT_32_63( (~0) & (~(LL_EXTI_LINE_36 | LL_EXTI_LINE_38)) );
}


static void Reset_BackupDomain()
{
	if ((LL_RCC_IsActiveFlag_PINRST() != FALSE) && (LL_RCC_IsActiveFlag_SFTRST() == FALSE))	{
		HAL_PWR_EnableBkUpAccess(); 		// Enable access to the RTC registers
		HAL_PWR_EnableBkUpAccess();			// Write the value twice to flush the APB-AHB bridge

		__HAL_RCC_BACKUPRESET_FORCE();
		__HAL_RCC_BACKUPRESET_RELEASE();
	}
}


static void Reset_IPCC()
{
	LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_IPCC);

	LL_C1_IPCC_ClearFlag_CHx(IPCC, LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);
	LL_C2_IPCC_ClearFlag_CHx(IPCC, LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);
	LL_C1_IPCC_DisableTransmitChannel(IPCC, LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);
	LL_C2_IPCC_DisableTransmitChannel(IPCC, LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);
	LL_C1_IPCC_DisableReceiveChannel(IPCC, LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);
	LL_C2_IPCC_DisableReceiveChannel(IPCC, LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);
}


static void Config_HSE(void)
{
	// Read HSE_Tuning from OTP
	OTP_ID0_t* p_otp = (OTP_ID0_t*) OTP_Read(0);
	if (p_otp) {
		LL_RCC_HSE_SetCapacitorTuning(p_otp->hse_tuning);
	}
}


static void Init_Rtc()
{
	LL_RTC_DisableWriteProtection(RTC);
	LL_RTC_WAKEUP_SetClock(RTC, CFG_RTC_WUCKSEL_DIVIDER);
	LL_RTC_EnableWriteProtection(RTC);
}


// Configures the system to be ready for low power mode
static void SystemPower_Config()
{
	// Select HSI as system clock source after Wake Up from Stop mode
	LL_RCC_SetClkAfterWakeFromStop(LL_RCC_STOP_WAKEUPCLOCK_HSI);

	UTIL_LPM_Init();								// Initialize low power manager
	LL_C2_PWR_SetPowerMode(LL_PWR_MODE_SHUTDOWN);	// Initialize the CPU2 reset value before starting CPU2 with C2BOOT

#if (CFG_USB_INTERFACE_ENABLE != 0)
	// Enable USB power
	HAL_PWREx_EnableVddUSB();
#endif
}


static void appe_Tl_Init()
{
	TL_MM_Config_t tl_mm_config;
	SHCI_TL_HciInitConf_t SHci_Tl_Init_Conf;

	TL_Init();	// Reference table initialization

	// System channel initialization
	UTIL_SEQ_RegTask(1 << CFG_TASK_SYSTEM_HCI_ASYNCH_EVT_ID, UTIL_SEQ_RFU, shci_user_evt_proc);
	SHci_Tl_Init_Conf.p_cmdbuffer = (uint8_t*)&SystemCmdBuffer;
	SHci_Tl_Init_Conf.StatusNotCallBack = APPE_SysStatusNot;
	shci_init(APPE_SysUserEvtRx, (void*) &SHci_Tl_Init_Conf);

	// Memory Manager channel initialization
	tl_mm_config.p_BleSpareEvtBuffer = BleSpareEvtBuffer;
	tl_mm_config.p_SystemSpareEvtBuffer = SystemSpareEvtBuffer;
	tl_mm_config.p_AsynchEvtPool = EvtPool;
	tl_mm_config.AsynchEvtPoolSize = POOL_SIZE;
	TL_MM_Init(&tl_mm_config);

	TL_Enable();
}

static void APPE_SysStatusNot(SHCI_TL_CmdStatus_t status)
{
	UNUSED(status);
	return;
}

/**
 * The type of the payload for a system user event is tSHCI_UserEvtRxParam
 * When the system event is both :
 *    - a ready event (subevtcode = SHCI_SUB_EVT_CODE_READY)
 *    - reported by the FUS (sysevt_ready_rsp == FUS_FW_RUNNING)
 * The buffer shall not be released
 * (eg ((tSHCI_UserEvtRxParam*)pPayload)->status shall be set to SHCI_TL_UserEventFlow_Disable)
 * When the status is not filled, the buffer is released by default
 */
static void APPE_SysUserEvtRx(void* pPayload)
{
	UNUSED(pPayload);
	// Traces channel initialization
	APPD_EnableCPU2();

	APP_BLE_Init();
	UTIL_LPM_SetOffMode(1 << CFG_LPM_APP, UTIL_LPM_ENABLE);
	return;
}


void MX_APPE_Process(void)
{
	UTIL_SEQ_Run(UTIL_SEQ_DEFAULT);
}


void UTIL_SEQ_Idle()
{
#if (CFG_LPM_SUPPORTED == 1)
	UTIL_LPM_EnterLowPower();
#endif
}


// This function is called by the scheduler each time an event is pending.
void UTIL_SEQ_EvtIdle(UTIL_SEQ_bm_t task_id_bm, UTIL_SEQ_bm_t evt_waited_bm)
{
	UTIL_SEQ_Run(UTIL_SEQ_DEFAULT);
}


void shci_notify_asynch_evt(void* pdata)
{
	UTIL_SEQ_SetTask(1 << CFG_TASK_SYSTEM_HCI_ASYNCH_EVT_ID, CFG_SCH_PRIO_0);
}


void shci_cmd_resp_release(uint32_t flag)
{
	UTIL_SEQ_SetEvt(1 << CFG_IDLEEVT_SYSTEM_HCI_CMD_EVT_RSP_ID);
}


void shci_cmd_resp_wait(uint32_t timeout)
{
	UTIL_SEQ_WaitEvt(1 << CFG_IDLEEVT_SYSTEM_HCI_CMD_EVT_RSP_ID);
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	switch (GPIO_Pin)
	{
		case BUTTON_SW1_Pin:
			APP_BLE_Key_Button1_Action();
			break;

		case BUTTON_SW2_Pin:
			APP_BLE_Key_Button2_Action();
			break;

		case BUTTON_SW3_Pin:
			APP_BLE_Key_Button3_Action();
			break;

		default:
			break;
	}
}
