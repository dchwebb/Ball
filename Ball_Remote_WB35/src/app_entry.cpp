#include "app_entry.h"
#include "BleApp.h"
#include "stm32_seq.h"
#include "shci_tl.h"


RTC_HandleTypeDef hrtc;

#define POOL_SIZE (CFG_TLBLE_EVT_QUEUE_LENGTH * 4U * DIVC((sizeof(TL_PacketHeader_t) + TL_BLE_EVENT_FRAME_SIZE), 4U))

PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t EvtPool[POOL_SIZE];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static TL_CmdPacket_t SystemCmdBuffer;
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t SystemSpareEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255U];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t BleSpareEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255];

static void APPE_SysStatusNotify(SHCI_TL_CmdStatus_t status);
static void APPE_SysUserEvtRx(void* pPayload);


void APPE_Init()
{
	// Initialize the TimerServer - manages switching between advertising states
	hrtc.Instance = RTC;
	hrtc.State = HAL_RTC_STATE_READY;
	HW_TS_Init(hw_ts_InitMode_Full, &hrtc);

	// Initialize transport layers
	TL_Init();

	// System channel initialization
	UTIL_SEQ_RegTask(1 << CFG_TASK_SYSTEM_HCI_ASYNCH_EVT_ID, UTIL_SEQ_RFU, shci_user_evt_proc);
	SHCI_TL_HciInitConf_t SHci_Tl_Init_Conf;
	SHci_Tl_Init_Conf.p_cmdbuffer = (uint8_t*)&SystemCmdBuffer;
	SHci_Tl_Init_Conf.StatusNotCallBack = APPE_SysStatusNotify;
	shci_init(APPE_SysUserEvtRx, (void*)&SHci_Tl_Init_Conf);

	// Memory Manager channel initialization
	TL_MM_Config_t tl_mm_config;
	tl_mm_config.p_BleSpareEvtBuffer = BleSpareEvtBuffer;
	tl_mm_config.p_SystemSpareEvtBuffer = SystemSpareEvtBuffer;
	tl_mm_config.p_AsynchEvtPool = EvtPool;
	tl_mm_config.AsynchEvtPoolSize = POOL_SIZE;
	TL_MM_Init(&tl_mm_config);

	HW_IPCC_Enable();

	// From now, the application is waiting for the ready event (VS_HCI_C2_Ready) received on the system channel before starting the Stack
	// This system event is received with APPE_SysUserEvtRx()
}


static void APPE_SysUserEvtRx(void* pPayload)
{
	bleApp.Init();
}


void RunPendingTasks(void)
{
	UTIL_SEQ_Run(UTIL_SEQ_DEFAULT);
}


void UTIL_SEQ_Idle()
{
	if (bleApp.sleepState == BleApp::SleepState::RequestSleep) {
		UTIL_SEQ_SetTask(1 << CFG_TASK_GoToSleep, CFG_SCH_PRIO_0);
	}
}


// This function is called by the scheduler each time an event is pending.
void UTIL_SEQ_EvtIdle(UTIL_SEQ_bm_t task_id_bm, UTIL_SEQ_bm_t evt_waited_bm)
{
	UTIL_SEQ_Run(UTIL_SEQ_DEFAULT);
}


static void APPE_SysStatusNotify(SHCI_TL_CmdStatus_t status)
{
	return;
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


