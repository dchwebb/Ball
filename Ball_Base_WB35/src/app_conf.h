#pragma once

#include "hw.h"
#include "hw_if.h"

/******************************************************************************
 * Application Config
 ******************************************************************************/
#define CONN_L(x) ((int)((x)/0.625f))
#define CONN_P(x) ((int)((x)/1.25f))
#define SCAN_P (0x320)
#define SCAN_L (0x320)
#define CONN_P1   (CONN_P(50))
#define CONN_P2   (CONN_P(100))
#define SUPERV_TIMEOUT (0x1F4)
#define CONN_L1   (CONN_L(10))
#define CONN_L2   (CONN_L(10))

/******************************************************************************
 * BLE Stack
 ******************************************************************************/

// Maximum number of simultaneous connections that the device will support (1 to 8)
#define CFG_BLE_NUM_LINK            2

// Maximum supported ATT_MTU size. Parameter is ignored by the CPU2 when CFG_BLE_OPTIONS is set to 1"
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
 * This parameter is ignored by the CPU2 when CFG_BLE_OPTIONS is set to 1"
 */
#define CFG_BLE_ATT_VALUE_ARRAY_SIZE    (1344)

// Prepare Write List size in terms of number of packet. Parameter is ignored by the CPU2 when CFG_BLE_OPTIONS is set to 1
#define CFG_BLE_PREPARE_WRITE_LIST_SIZE         BLE_PREP_WRITE_X_ATT(CFG_BLE_MAX_ATT_MTU)


// Number of allocated memory blocks. Parameter is overwritten by CPU2 with hardcoded optimal value when CFG_BLE_OPTIONS is set to 1
#define CFG_BLE_MBLOCK_COUNT            (BLE_MBLOCKS_CALC(CFG_BLE_PREPARE_WRITE_LIST_SIZE, CFG_BLE_MAX_ATT_MTU, CFG_BLE_NUM_LINK))





/**
 * BLE stack Options flags to be configured with:
 * - SHCI_C2_BLE_INIT_OPTIONS_LL_ONLY
 * - SHCI_C2_BLE_INIT_OPTIONS_LL_HOST
 * - SHCI_C2_BLE_INIT_OPTIONS_NO_SVC_CHANGE_DESC
 * - SHCI_C2_BLE_INIT_OPTIONS_WITH_SVC_CHANGE_DESC
 * - SHCI_C2_BLE_INIT_OPTIONS_DEVICE_NAME_RO
 * - SHCI_C2_BLE_INIT_OPTIONS_DEVICE_NAME_RW
 * - SHCI_C2_BLE_INIT_OPTIONS_POWER_CLASS_1
 * - SHCI_C2_BLE_INIT_OPTIONS_POWER_CLASS_2_3
 * which are used to set following configuration bits:
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
#define CFG_BLE_OPTIONS  (SHCI_C2_BLE_INIT_OPTIONS_LL_HOST | SHCI_C2_BLE_INIT_OPTIONS_WITH_SVC_CHANGE_DESC | SHCI_C2_BLE_INIT_OPTIONS_DEVICE_NAME_RW | SHCI_C2_BLE_INIT_OPTIONS_NO_EXT_ADV | SHCI_C2_BLE_INIT_OPTIONS_NO_CS_ALGO2 | SHCI_C2_BLE_INIT_OPTIONS_FULL_GATTDB_NVM | SHCI_C2_BLE_INIT_OPTIONS_GATT_CACHING_NOTUSED | SHCI_C2_BLE_INIT_OPTIONS_POWER_CLASS_2_3)

/**
 * BLE stack Options_extension flags to be configured with:
 * - SHCI_C2_BLE_INIT_OPTIONS_APPEARANCE_WRITABLE
 * - SHCI_C2_BLE_INIT_OPTIONS_APPEARANCE_READONLY
 * - SHCI_C2_BLE_INIT_OPTIONS_ENHANCED_ATT_SUPPORTED
 * - SHCI_C2_BLE_INIT_OPTIONS_ENHANCED_ATT_NOTSUPPORTED
 * which are used to set following configuration bits:
 * (bit 0): 1: appearance Writable
 *          0: appearance Read-Only
 * (bit 1): 1: Enhanced ATT supported
 *          0: Enhanced ATT not supported
 * other bits: reserved (shall be set to 0)
 */
#define CFG_BLE_OPTIONS_EXT  (SHCI_C2_BLE_INIT_OPTIONS_APPEARANCE_READONLY | SHCI_C2_BLE_INIT_OPTIONS_ENHANCED_ATT_NOTSUPPORTED)




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
#define CFG_TLBLE_EVT_QUEUE_LENGTH 8
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

#define CFG_TS_TICK_VAL           DIVR( (CFG_RTCCLK_DIV * 1000000), LSE_VALUE )			// tick timer value in us




/******************************************************************************
 * Scheduler
 ******************************************************************************/

/**
 * These are the lists of task id registered to the scheduler
 * Each task id shall be in the range [0:31]
 * This mechanism allows to implement a generic code in the API TL_BLE_HCI_StatusNot() to comply with
 * the requirement that a HCI/ACI command shall never be sent if there is already one pending
 */

// Tasks that may send an ACI/HCI command
typedef enum
{
    CFG_TASK_ScanRequest,
    CFG_TASK_ConnectRequest,
    CFG_TASK_DisconnectRequest,
	CFG_TASK_HIDServiceDiscovery,
	CFG_TASK_GetBatteryLevel,
	CFG_TASK_GyroCommand,
	CFG_TASK_ReadGyroRegister,
    CFG_TASK_CONN_UPDATE_ID,
    CFG_TASK_HCI_ASYNCH_EVT_ID,
    CFG_LAST_TASK_ID_WITH_HCICMD,                                               // Last in the list
} CFG_Task_Id_With_HCI_Cmd_t;

// Tasks that do not send ACI/HCI commands
typedef enum {
    CFG_FIRST_TASK_ID_WITH_NO_HCICMD = CFG_LAST_TASK_ID_WITH_HCICMD - 1,        // FIRST in the list
    CFG_TASK_SYSTEM_HCI_ASYNCH_EVT_ID,
    CFG_LAST_TASK_ID_WITHO_NO_HCICMD                                            // LAST in the list
} CFG_Task_Id_With_NO_HCI_Cmd_t;
#define CFG_TASK_NBR    CFG_LAST_TASK_ID_WITHO_NO_HCICMD

// This is the list of priority required by the application (0-31)
typedef enum
{
    CFG_SCH_PRIO_0,
    CFG_PRIO_NBR,
} CFG_SCH_Prio_Id_t;

// This is a 32 bit bitmap of events id supported in the application
typedef enum
{
    CFG_IDLEEVT_HCI_CMD_EVT_RSP_ID,
    CFG_IDLEEVT_SYSTEM_HCI_CMD_EVT_RSP_ID,
} CFG_IdleEvt_Id_t;



/******************************************************************************
 * OTP manager
 ******************************************************************************/
#define CFG_OTP_BASE_ADDRESS    OTP_AREA_BASE

#define CFG_OTP_END_ADRESS      OTP_AREA_END_ADDR


