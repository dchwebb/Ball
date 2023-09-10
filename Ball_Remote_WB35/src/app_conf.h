#pragma once

#include "hw.h"
#include "hw_conf.h"
#include "hw_if.h"

/**
 * BLE stack Options flags:
 * (bit 0): 1: LL only
 *          0: LL + host
 * (bit 1): 1: no service change desc.
 *          0: with service change desc.
 * (bit 2): 1: device name Read-Only
 *          0: device name R/W
 * (bit 7): 1: LE Power Class 1
 *          0: LE Power Class 2-3
 * other bits: reserved (shall be set to 0)
 */
#define CFG_BLE_OPTIONS  (SHCI_C2_BLE_INIT_OPTIONS_LL_HOST | SHCI_C2_BLE_INIT_OPTIONS_WITH_SVC_CHANGE_DESC | SHCI_C2_BLE_INIT_OPTIONS_DEVICE_NAME_RW | SHCI_C2_BLE_INIT_OPTIONS_POWER_CLASS_2_3)



/**
 * BLE stack Options_extension flags:
 * (bit 0): 1: appearance Writable
 *          0: appearance Read-Only
 * (bit 1): 1: Enhanced ATT supported
 *          0: Enhanced ATT not supported
 * other bits: reserved (shall be set to 0)
 */
#define CFG_BLE_OPTIONS_EXT  (SHCI_C2_BLE_INIT_OPTIONS_APPEARANCE_READONLY | SHCI_C2_BLE_INIT_OPTIONS_ENHANCED_ATT_NOTSUPPORTED)

// Maximum number of simultaneous connections that the device will support (1 to 8)
#define CFG_BLE_NUM_LINK            2

// Maximum supported ATT_MTU size: ignored by the CPU2 when CFG_BLE_OPTIONS has SHCI_C2_BLE_INIT_OPTIONS_LL_ONLY flag set
#define CFG_BLE_MAX_ATT_MTU             (156)

/**
 * Size of the storage area for Attribute values
 *  This value depends on the number of attributes used by application. In particular the sum of the following quantities (in octets) should be made for each attribute:
 *  - attribute value length
 *  - 5, if UUID is 16 bit; 19, if UUID is 128 bit
 *  - 2, if server configuration descriptor is used
 *  - 2*DTM_NUM_LINK, if client configuration descriptor is used
 *  - 2, if extended properties is used
 *  The total amount of memory needed is the sum of the above quantities for each attribute.
 * This parameter is ignored by the CPU2 when CFG_BLE_OPTIONS has SHCI_C2_BLE_INIT_OPTIONS_LL_ONLY flag set
 */
#define CFG_BLE_ATT_VALUE_ARRAY_SIZE    (1344)

// Prepare Write List size in terms of number of packet
// This parameter is ignored by the CPU2 when CFG_BLE_OPTIONS has SHCI_C2_BLE_INIT_OPTIONS_LL_ONLY flag set
#define CFG_BLE_PREPARE_WRITE_LIST_SIZE         BLE_PREP_WRITE_X_ATT(CFG_BLE_MAX_ATT_MTU)

// Number of allocated memory blocks - overwritten by the CPU2 with an hardcoded optimal value when the parameter when CFG_BLE_OPTIONS is set to 1
 #define CFG_BLE_MBLOCK_COUNT            (BLE_MBLOCKS_CALC(CFG_BLE_PREPARE_WRITE_LIST_SIZE, CFG_BLE_MAX_ATT_MTU, CFG_BLE_NUM_LINK))



/******************************************************************************
 * Transport Layer
 ******************************************************************************/
/**
 * Queue length of BLE Event
 * This parameter defines the number of asynchronous events that can be stored in the HCI layer before
 * being reported to the application. When a command is sent to the BLE core coprocessor, the HCI layer
 * is waiting for the event with the Num_HCI_Command_Packets set to 1. The receive queue shall be large
 * enough to store all asynchronous events received in between.
 * When CFG_TLBLE_MOST_EVENT_PAYLOAD_SIZE is set to 27, this allow to store three 255 bytes long asynchronous events
 * between the HCI command and its event.
 * This parameter depends on the value given to CFG_TLBLE_MOST_EVENT_PAYLOAD_SIZE. When the queue size is to small,
 * the system may hang if the queue is full with asynchronous events and the HCI layer is still waiting
 * for a CC/CS event, In that case, the notification TL_BLE_HCI_ToNot() is called to indicate
 * to the application a HCI command did not receive its command event within 30s (Default HCI Timeout).
 */
#define CFG_TLBLE_EVT_QUEUE_LENGTH 5
/**
 * This parameter should be set to fit most events received by the HCI layer. It defines the buffer size of each element
 * allocated in the queue of received events and can be used to optimize the amount of RAM allocated by the Memory Manager.
 * It should not exceed 255 which is the maximum HCI packet payload size (a greater value is a lost of memory as it will
 * never be used)
 * It shall be at least 4 to receive the command status event in one frame.
 * The default value is set to 27 to allow receiving an event of MTU size in a single buffer. This value maybe reduced
 * further depending on the application.
 */
#define CFG_TLBLE_MOST_EVENT_PAYLOAD_SIZE 255   /**< Set to 255 with the memory manager and the mailbox */

#define TL_BLE_EVENT_FRAME_SIZE ( TL_EVT_HDR_SIZE + CFG_TLBLE_MOST_EVENT_PAYLOAD_SIZE )



/******************************************************************************
 * RTC interface
 ******************************************************************************/
//#define HAL_RTCEx_WakeUpTimerIRQHandler(...)  HW_TS_RTC_Wakeup_Handler( )

/******************************************************************************
 * Timer Server
 ******************************************************************************/
/**
 *  CFG_RTC_WUCKSEL_DIVIDER:  This sets the RTCCLK divider to the wakeup timer.
 *  The lower is the value, the better is the power consumption and the accuracy of the timerserver
 *  The higher is the value, the finest is the granularity
 *
 *  CFG_RTC_ASYNCH_PRESCALER: This sets the asynchronous prescaler of the RTC. It should as high as possible ( to output
 *  clock as low as possible) but the output clock should be equal or higher frequency compare to the clock feeding
 *  the wakeup timer. A lower clock speed would impact the accuracy of the timer server.
 *
 *  CFG_RTC_SYNCH_PRESCALER: This sets the synchronous prescaler of the RTC.
 *  When the 1Hz calendar clock is required, it shall be sets according to other settings
 *  When the 1Hz calendar clock is not needed, CFG_RTC_SYNCH_PRESCALER should be set to 0x7FFF (MAX VALUE)
 *
 *  CFG_RTCCLK_DIVIDER_CONF:
 *  Shall be set to either 0,2,4,8,16
 *  When set to either 2,4,8,16, the 1Hhz calendar is supported
 *  When set to 0, the user sets its own configuration
 *
 *  The following settings are computed with LSI as input to the RTC
 */

#define CFG_RTCCLK_DIVIDER_CONF 0

#if (CFG_RTCCLK_DIVIDER_CONF == 0)
/**
 * Custom configuration
 * It does not support 1Hz calendar
 * It divides the RTC CLK by 16
 */

#define CFG_RTCCLK_DIV  (16)
#define CFG_RTC_WUCKSEL_DIVIDER (0)
#define CFG_RTC_ASYNCH_PRESCALER (0x0F)
#define CFG_RTC_SYNCH_PRESCALER (0x7FFF)

#else

#if (CFG_RTCCLK_DIVIDER_CONF == 2)
/**
 * It divides the RTC CLK by 2
 */
#define CFG_RTC_WUCKSEL_DIVIDER (3)
#endif

#if (CFG_RTCCLK_DIVIDER_CONF == 4)
/**
 * It divides the RTC CLK by 4
 */
#define CFG_RTC_WUCKSEL_DIVIDER (2)
#endif

#if (CFG_RTCCLK_DIVIDER_CONF == 8)
/**
 * It divides the RTC CLK by 8
 */
#define CFG_RTC_WUCKSEL_DIVIDER (1)
#endif

#if (CFG_RTCCLK_DIVIDER_CONF == 16)
/**
 * It divides the RTC CLK by 16
 */
#define CFG_RTC_WUCKSEL_DIVIDER (0)
#endif

#define CFG_RTCCLK_DIV              CFG_RTCCLK_DIVIDER_CONF
#define CFG_RTC_ASYNCH_PRESCALER    (CFG_RTCCLK_DIV - 1)
#define CFG_RTC_SYNCH_PRESCALER     (DIVR( LSE_VALUE, (CFG_RTC_ASYNCH_PRESCALER+1) ) - 1 )

#endif

/** tick timer value in us */
#define CFG_TS_TICK_VAL           DIVR( (CFG_RTCCLK_DIV * 1000000), LSE_VALUE )

typedef enum
{
	CFG_TIM_PROC_ID_ISR,
} CFG_TimProcID_t;


#define APP_DBG_MSG                 printf




/******************************************************************************
 * Scheduler
 ******************************************************************************/
// These are the lists of task ID in range [0:31] registered to the scheduler
// This mechanism allows to implement a generic code in the API TL_BLE_HCI_StatusNot() to comply with
// the requirement that a HCI/ACI command shall never be sent if there is already one pending

// List of tasks that send ACI/HCI commands
typedef enum {
    CFG_TASK_SwitchLPAdvertising,
	CFG_TASK_SwitchFastAdvertising,
    CFG_TASK_CancelAdvertising,
	CFG_TASK_GoToSleep,
	CFG_TASK_EnterSleepMode,
    CFG_TASK_HCI_ASYNCH_EVT_ID,
	CFG_TASK_JOYSTICK_NOTIFICATION,
	CFG_TASK_BATTERY_NOTIFICATION,
    CFG_LAST_TASK_ID_WITH_HCICMD,                                               // Shall be LAST in the list
} CFG_Task_Id_With_HCI_Cmd_t;

// List of tasks that don't send ACI/HCI commands
typedef enum {
    CFG_FIRST_TASK_ID_WITH_NO_HCICMD = CFG_LAST_TASK_ID_WITH_HCICMD - 1,        // Shall be FIRST in the list
    CFG_TASK_SYSTEM_HCI_ASYNCH_EVT_ID,
    CFG_LAST_TASK_ID_WITHO_NO_HCICMD                                            // Shall be LAST in the list
} CFG_Task_Id_With_NO_HCI_Cmd_t;
#define CFG_TASK_NBR    CFG_LAST_TASK_ID_WITHO_NO_HCICMD

// Priority list in  range 0..31
typedef enum {
    CFG_SCH_PRIO_0,
    CFG_PRIO_NBR,
} CFG_SCH_Prio_Id_t;

// This is a bit mapping over 32bits listing all events id supported in the application
typedef enum {
    CFG_IDLEEVT_HCI_CMD_EVT_RSP_ID,
    CFG_IDLEEVT_SYSTEM_HCI_CMD_EVT_RSP_ID,
} CFG_IdleEvt_Id_t;

// Bit map of Low Power Manager supported requesters (up to 32)
 typedef enum {
    CFG_LPM_APP,
    CFG_LPM_APP_BLE,
} CFG_LPM_Id_t;


// OTP manager
#define CFG_OTP_BASE_ADDRESS    OTP_AREA_BASE
#define CFG_OTP_END_ADRESS      OTP_AREA_END_ADDR

