#include "main.h"
#include "app_common.h"
#include "dbg_trace.h"
#include "ble.h"
#include "stm32_seq.h"
#include "app_ble.h"
#include "ble_hid.h"


typedef enum {
	HID_NOTIFICATION_RECEIVED_EVT,
	BATTERY_NOTIFICATION_RECEIVED_EVT,
} HID_Client_Opcode_Notification_evt_t;

typedef struct  {
	int16_t x;
	int16_t y;
	int16_t z;
} Position3D_t;

typedef struct {
	HID_Client_Opcode_Notification_evt_t  P2P_Client_Evt_Opcode;
	uint8_t* Payload;
	uint8_t  Length;
} HID_Client_App_Notification_evt_t;

typedef struct {
	HidState state;								// state of the HID Client state machine
	uint16_t connHandle;						// connection handle
	uint16_t HIDServiceHandle;					// handle of the HID service
	uint16_t HIDServiceEndHandle;				// end handle of the HID service
	uint16_t HIDNotificationCharHdle;			// handle of the HID Report characteristic
	uint16_t HIDNotificationDescHandle;			// handle of the HID report client configuration descriptor
	uint16_t BatteryServiceHandle;				// handle of the Battery service
	uint16_t BatteryServiceEndHandle;			// end handle of the Battery service
	uint16_t BatteryNotificationCharHdle;		// handle of the Rx characteristic - Notification From Server
	uint16_t BatteryNotificationDescHandle;		// handle of the client configuration descriptor of Rx characteristic
} HID_ClientContext_t;


#define UNPACK_2_BYTE_PARAMETER(ptr)  \
		(uint16_t)((uint16_t)(*((uint8_t *)ptr))) |   \
		(uint16_t)((((uint16_t)(*((uint8_t *)ptr + 1))) << 8))

static HID_ClientContext_t HIDClientContext;
//PLACE_IN_SECTION("BLE_APP_CONTEXT") static HID_ClientContext_t HIDClientContext;

Position3D_t position3D;
//PLACE_IN_SECTION("BLE_APP_CONTEXT") Position3D_t position3D;

static void Gatt_Notification(HID_Client_App_Notification_evt_t *pNotification);
static SVCCTL_EvtAckStatus_t HIDEventHandler(void *Event);
static void Update_Service();


void HID_APP_Init(void)
{
	UTIL_SEQ_RegTask(1 << CFG_TASK_SEARCH_SERVICE_ID, UTIL_SEQ_RFU, Update_Service);

	HIDClientContext.state = HidState::Idle;

	SVCCTL_RegisterCltHandler(HIDEventHandler);	//  Register the event handler to the BLE controller
	APP_DBG_MSG("-- P2P CLIENT INITIALIZED \n");
	return;
}


void HIDConnectionNotification(P2PC_APP_ConnHandle_Not_evt_t *pNotification)
{
	switch (pNotification->P2P_Evt_Opcode) {

	case PEER_CONN_HANDLE_EVT:
		// Reset state of client
		HIDClientContext.BatteryNotificationCharHdle = 0;
		HIDClientContext.BatteryNotificationDescHandle = 0;
		HIDClientContext.BatteryServiceHandle = 0;
		HIDClientContext.HIDNotificationCharHdle = 0;
		HIDClientContext.HIDNotificationDescHandle = 0;
		HIDClientContext.HIDServiceHandle = 0;

		HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
		break;

	case PEER_DISCON_HANDLE_EVT:
		HIDClientContext.state = HidState::Idle;
		HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
		break;

	default:
		break;
	}
	return;
}


void HID_APP_SW1_Button_Action(void)
{
	UTIL_SEQ_SetTask(1 << CFG_TASK_SW1_BUTTON_PUSHED_ID, CFG_SCH_PRIO_0);
}


static SVCCTL_EvtAckStatus_t HIDEventHandler(void *Event)
{
	SVCCTL_EvtAckStatus_t return_value;
	hci_event_pckt *event_pckt;
	evt_blecore_aci *blecore_evt;

	HID_Client_App_Notification_evt_t Notification;

	return_value = SVCCTL_EvtNotAck;
	event_pckt = (hci_event_pckt *)(((hci_uart_pckt*)Event)->data);

	switch(event_pckt->evt)
	{
	case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
	{
		blecore_evt = (evt_blecore_aci*)event_pckt->data;
		switch (blecore_evt->ecode)
		{

		case ACI_ATT_READ_BY_GROUP_TYPE_RESP_VSEVT_CODE:
		{
			// Result of discover all primary services
			aci_att_read_by_group_type_resp_event_rp0 *pr = (aci_att_read_by_group_type_resp_event_rp0*)blecore_evt->data;

			if (HIDClientContext.state != HidState::Idle) {
				BleApplication::ConnectionStatus status = bleApp.GetClientConnectionStatus(HIDClientContext.connHandle);

				// Handle disconnected
				if ((HIDClientContext.state == HidState::ClientConnected) && (status == BleApplication::ConnectionStatus::Idle)) {
					HIDClientContext.state = HidState::Idle;
					HIDClientContext.connHandle = 0xFFFF;
					break;
				}
			}

			if (HIDClientContext.state == HidState::Idle) {
				HIDClientContext.connHandle = pr->Connection_Handle;

				uint8_t numServ = (pr->Data_Length) / pr->Attribute_Data_Length;

				APP_DBG_MSG("-- GATT : Services: %d; Connection handle 0x%x \n", numServ, HIDClientContext.connHandle);

				// Event data: 2 bytes start handle | 2 bytes end handle | 2 or 16 bytes data
				// Only if the UUID is 16 bit so check if the data length is 6
				if (pr->Attribute_Data_Length == 6) {
					uint8_t idx = 4;
					for (uint8_t i = 0; i < numServ; i++) {
						uint16_t uuid = UNPACK_2_BYTE_PARAMETER(&pr->Attribute_Data_List[idx]);

						if (uuid == BATTERY_SERVICE_UUID) {
							HIDClientContext.BatteryServiceHandle = UNPACK_2_BYTE_PARAMETER(&pr->Attribute_Data_List[idx - 4]);
							HIDClientContext.BatteryServiceEndHandle = UNPACK_2_BYTE_PARAMETER (&pr->Attribute_Data_List[idx - 2]);
							HIDClientContext.state = HidState::DiscoverCharacs;
							APP_DBG_MSG("-- GATT : Battery Service UUID: 0x%x\n", uuid);
						} else if (uuid == HUMAN_INTERFACE_DEVICE_SERVICE_UUID) {
							HIDClientContext.HIDServiceHandle = UNPACK_2_BYTE_PARAMETER(&pr->Attribute_Data_List[idx - 4]);
							HIDClientContext.HIDServiceEndHandle = UNPACK_2_BYTE_PARAMETER (&pr->Attribute_Data_List[idx - 2]);
							HIDClientContext.state = HidState::DiscoverCharacs;
							APP_DBG_MSG("-- GATT : HID Service UUID: 0x%x\n", uuid);
						} else {
							APP_DBG_MSG("-- GATT : Service UUID: 0x%x\n", uuid);
						}

						idx += 6;
					}
				}
			}
		}
		break;

		case ACI_ATT_READ_BY_TYPE_RESP_VSEVT_CODE:
		{
			// Triggered after discover characteristics
			aci_att_read_by_type_resp_event_rp0 *pr = (aci_att_read_by_type_resp_event_rp0*)blecore_evt->data;

			if (HIDClientContext.connHandle == pr->Connection_Handle) {
				// Event data: 2 bytes start handle | 1 byte char properties | 2 bytes handle | 2 or 16 bytes data
				// Only handle 16 bit UUIDs
				uint8_t idx = 5;
				if (pr->Handle_Value_Pair_Length == 7) {

					while (pr->Data_Length > 0) {
						uint16_t uuid = UNPACK_2_BYTE_PARAMETER(&pr->Handle_Value_Pair_Data[idx]);
						// store the characteristic handle not the attribute handle
						uint16_t handle = UNPACK_2_BYTE_PARAMETER(&pr->Handle_Value_Pair_Data[idx - 2]);

						APP_DBG_MSG("-- GATT : uuid %x handle: %x\n", uuid, handle);

						if (uuid == BATTERY_LEVEL_CHAR_UUID) {
							APP_DBG_MSG("-- GATT : Battery Notification Found\n");
							HIDClientContext.state = HidState::DiscoverNotificationCharDesc;
							HIDClientContext.BatteryNotificationCharHdle = handle;
						}
						if (uuid == REPORT_CHAR_UUID) {
							APP_DBG_MSG("-- GATT : HID Report Notification Found\n");
							HIDClientContext.state = HidState::DiscoverNotificationCharDesc;
							HIDClientContext.HIDNotificationCharHdle = handle;
						}
						pr->Data_Length -= 7;
						idx += 7;

					}
					// To avoid continually retriggering discover characteristics
					if (HIDClientContext.state == HidState::DiscoverCharacs) {
						HIDClientContext.state = HidState::DiscoveringCharacs;
					}
				}
			}
		}
		break;

		case ACI_ATT_FIND_INFO_RESP_VSEVT_CODE:
		{
			// After finding client configuration descriptor to activate notify event
			aci_att_find_info_resp_event_rp0 *pr = (aci_att_find_info_resp_event_rp0*)blecore_evt->data;

			if (HIDClientContext.connHandle == pr->Connection_Handle) {
				uint8_t numDesc = (pr->Event_Data_Length) / 4;
				APP_DBG_MSG("-- GATT : Found %d client configuration descriptors\n", numDesc);

				// event data: 2 bytes handle | 2 bytes UUID
				uint8_t idx = 0;
				if (pr->Format == UUID_TYPE_16) {
					for (uint8_t i = 0; i < numDesc; i++) {
						uint16_t handle = UNPACK_2_BYTE_PARAMETER(&pr->Handle_UUID_Pair[idx]);
						uint16_t uuid = UNPACK_2_BYTE_PARAMETER(&pr->Handle_UUID_Pair[idx + 2]);

						if (uuid == CLIENT_CHAR_CONFIG_DESCRIPTOR_UUID && handle > HIDClientContext.BatteryNotificationCharHdle && handle <= HIDClientContext.BatteryServiceEndHandle) {
							APP_DBG_MSG("-- GATT : Battery Client Char Config Descriptor: 0x%x\n", handle);
							HIDClientContext.BatteryNotificationDescHandle = handle;
							HIDClientContext.state = HidState::EnableNotificationDesc;

						} else if (uuid == CLIENT_CHAR_CONFIG_DESCRIPTOR_UUID && handle > HIDClientContext.HIDNotificationCharHdle && handle <= HIDClientContext.HIDServiceEndHandle) {
							APP_DBG_MSG("-- GATT : HID Client Char Config Descriptor: 0x%x\n", handle);
							HIDClientContext.HIDNotificationDescHandle = handle;
							HIDClientContext.state = HidState::EnableNotificationDesc;
						}

						idx += 4;
					}
				}
			}
		}
		break;

		case ACI_GATT_NOTIFICATION_VSEVT_CODE:
		{
			aci_gatt_notification_event_rp0 *pr = (aci_gatt_notification_event_rp0*)blecore_evt->data;

			if (HIDClientContext.connHandle == pr->Connection_Handle) {

				if (pr->Attribute_Handle == HIDClientContext.BatteryNotificationCharHdle) {
					Notification.P2P_Client_Evt_Opcode = BATTERY_NOTIFICATION_RECEIVED_EVT;
					Notification.Length = pr->Attribute_Value_Length;
					Notification.Payload = &pr->Attribute_Value[0];

					Gatt_Notification(&Notification);
				}
				if (pr->Attribute_Handle == HIDClientContext.HIDNotificationCharHdle) {
					Notification.P2P_Client_Evt_Opcode = HID_NOTIFICATION_RECEIVED_EVT;
					Notification.Length = pr->Attribute_Value_Length;
					Notification.Payload = &pr->Attribute_Value[0];

					Gatt_Notification(&Notification);
				}

			}
		}
		break;

		case ACI_GATT_PROC_COMPLETE_VSEVT_CODE:
		{
			aci_gatt_proc_complete_event_rp0 *pr = (aci_gatt_proc_complete_event_rp0*)blecore_evt->data;
			APP_DBG_MSG("-- GATT : ACI_GATT_PROC_COMPLETE_VSEVT_CODE \n\n");

			if (HIDClientContext.connHandle == pr->Connection_Handle) {
				UTIL_SEQ_SetTask(1 << CFG_TASK_SEARCH_SERVICE_ID, CFG_SCH_PRIO_0);		// Triggers Update_Service()
			}
		}
		break;

		default:
			break;
		}
	}

	break; /* HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE */

	default:
		break;
	}

	return (return_value);
}/* end BLE_CTRL_Event_Acknowledged_Status_t */


static inline int16_t Clamp(int16_t v, int16_t min, int16_t max)
{
	if (v > max)		return max;
	else if (v < min) 	return min;
	else 				return v;
}


void Gatt_Notification(HID_Client_App_Notification_evt_t *pNotification)
{
	if (pNotification->P2P_Client_Evt_Opcode == BATTERY_NOTIFICATION_RECEIVED_EVT) {
		APP_DBG_MSG(" -- P2P APPLICATION CLIENT: x: %d; y: %d; z: %d; Battery: %d%%\n", position3D.x, position3D.y, position3D.z, pNotification->Payload[0]);
	}
	if (pNotification->P2P_Client_Evt_Opcode == HID_NOTIFICATION_RECEIVED_EVT) {
		volatile int16_t* payload = (int16_t*)pNotification->Payload;
		position3D.x = payload[0];
		position3D.y = payload[1];
		position3D.z = payload[2];

		// Output to PWM - values vary from ~400 - 600
		//int16_t pwmX = Clamp(position3D.x - 410, 0, 205) * 20;
		int16_t pwmX = position3D.x;
		int16_t pwmY = position3D.y;
		int16_t pwmZ = position3D.z;

		TIM2->CCR1 = pwmX;
		TIM2->CCR2 = pwmY;
		TIM2->CCR3 = pwmZ;

		if (GPIOB->ODR & GPIO_ODR_OD8)			GPIOB->ODR &= ~GPIO_ODR_OD8;
		else                                    GPIOB->ODR |=  GPIO_ODR_OD8;


		//APP_DBG_MSG(" -- P2P APPLICATION CLIENT: HID x: %d; y: %d; z: %d\n", pwmX, payload[1], payload[2]);
	}

	return;
}


HidState HID_Client_APP_Get_State() {
	return HIDClientContext.state;
}


void Update_Service()
{
	// Triggered each time a GATT operation completes - keep track of discovery process status to get handles and activate notifications accordingly
	uint8_t enable = 1;
	//uint8_t disable = 0;

	uint8_t startHandle, endHandle;

	if (HIDClientContext.state != HidState::Idle) {

		switch (HIDClientContext.state)
		{

		case HidState::DiscoverServices:
			APP_DBG_MSG("P2P_DISCOVER_SERVICES\n");
			break;
		case HidState::DiscoverCharacs:
			APP_DBG_MSG("* GATT : Discover HID/Battery Characteristics\n");

			// Discover characteristics of both battery and HID services
			if (HIDClientContext.BatteryServiceHandle > HIDClientContext.HIDServiceHandle) {
				startHandle = HIDClientContext.HIDServiceHandle;
				endHandle = HIDClientContext.BatteryServiceEndHandle;
			} else {
				startHandle = HIDClientContext.BatteryServiceHandle;
				endHandle = HIDClientContext.HIDServiceEndHandle;
			}
			aci_gatt_disc_all_char_of_service(HIDClientContext.connHandle, startHandle, endHandle);

			break;

		case HidState::DiscoverNotificationCharDesc:
			APP_DBG_MSG("* GATT : Discover HID/Battery Notification Characteristic Descriptor\n");

			if (HIDClientContext.BatteryNotificationCharHdle > HIDClientContext.HIDNotificationCharHdle) {
				startHandle = HIDClientContext.HIDNotificationCharHdle;
				endHandle = HIDClientContext.BatteryNotificationCharHdle + 2;
			} else {
				startHandle = HIDClientContext.BatteryNotificationCharHdle;
				endHandle = HIDClientContext.HIDNotificationCharHdle + 2;
			}

			aci_gatt_disc_all_char_desc(HIDClientContext.connHandle, startHandle, endHandle);

			break;

		case HidState::EnableNotificationDesc:
			APP_DBG_MSG("* GATT : Enable Battery Notification\n");
			aci_gatt_write_char_desc(HIDClientContext.connHandle, HIDClientContext.BatteryNotificationDescHandle, 2, &enable);

			HIDClientContext.state = HidState::EnableHIDNotificationDesc;
			break;

		case HidState::EnableHIDNotificationDesc:
			APP_DBG_MSG("* GATT : Enable HID Notification\n");
			aci_gatt_write_char_desc(HIDClientContext.connHandle, HIDClientContext.HIDNotificationDescHandle, 2, &enable);

			HIDClientContext.state = HidState::ClientConnected;
			HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
			break;

//		case APP_BLE_DISABLE_NOTIFICATION_DESC:
//			APP_DBG_MSG("* GATT : Disable Battery Notification\n");
//			aci_gatt_write_char_desc(HIDClientContext.connHandle, HIDClientContext.BatteryNotificationDescHandle, 2, &disable);
//
//			HIDClientContext.state = HidState::ClientConnected;
//
//			break;
		default:
			break;
		}
	}
	return;
}
