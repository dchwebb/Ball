#include "main.h"
#include "app_common.h"
#include "dbg_trace.h"
#include "ble.h"
#include "stm32_seq.h"
#include "app_ble.h"
#include "ble_hid.h"
#include <Algorithm>

HidApp hidApp;

void HidApp::Init(void)
{
	UTIL_SEQ_RegTask(1 << CFG_TASK_SEARCH_SERVICE_ID, UTIL_SEQ_RFU, UpdateService);

	state = HidState::Idle;

	SVCCTL_RegisterCltHandler(HIDEventHandler);
	APP_DBG_MSG("-- HID CLIENT INITIALIZED \n");
}


void HidApp::HIDConnectionNotification()
{
	switch (bleApp.deviceConnectionStatus) {

	case BleApplication::ConnectionStatus::ClientConnected:
		// Reset state of client
		BatteryNotificationCharHdle = 0;
		BatteryNotificationDescHandle = 0;
		BatteryServiceHandle = 0;
		HIDNotificationCharHdle = 0;
		HIDNotificationDescHandle = 0;
		HIDServiceHandle = 0;

		HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
		break;

	case BleApplication::ConnectionStatus::Idle:
		state = HidState::Idle;
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


SVCCTL_EvtAckStatus_t HidApp::HIDEventHandler(void *Event)
{
	//evt_blecore_aci* blecore_evt;
	hci_event_pckt* event_pckt = (hci_event_pckt *)(((hci_uart_pckt*)Event)->data);

	switch (event_pckt->evt)
	{
	case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
	{
		evt_blecore_aci* blecore_evt = (evt_blecore_aci*)event_pckt->data;
		switch (blecore_evt->ecode)
		{

		case ACI_ATT_READ_BY_GROUP_TYPE_RESP_VSEVT_CODE:
		{
			// Result of discover all primary services
			aci_att_read_by_group_type_resp_event_rp0 *pr = (aci_att_read_by_group_type_resp_event_rp0*)blecore_evt->data;

			if (hidApp.state != HidState::Idle) {
				BleApplication::ConnectionStatus status = bleApp.GetClientConnectionStatus(hidApp.connHandle);

				// Handle disconnected
				if ((hidApp.state == HidState::ClientConnected) && (status == BleApplication::ConnectionStatus::Idle)) {
					hidApp.state = HidState::Idle;
					hidApp.connHandle = 0xFFFF;
					break;
				}
			}

			if (hidApp.state == HidState::Idle) {
				hidApp.connHandle = pr->Connection_Handle;

				uint8_t numServ = (pr->Data_Length) / pr->Attribute_Data_Length;

				APP_DBG_MSG("-- GATT : Services: %d; Connection handle 0x%x \n", numServ, hidApp.connHandle);

				// Event data: 2 bytes start handle | 2 bytes end handle | 2 or 16 bytes data
				// Only if the UUID is 16 bit so check if the data length is 6
				if (pr->Attribute_Data_Length == 6) {
					uint8_t idx = 4;
					for (uint8_t i = 0; i < numServ; i++) {
						uint16_t uuid = *((uint16_t*)&pr->Attribute_Data_List[idx]);

						if (uuid == BATTERY_SERVICE_UUID) {
							hidApp.BatteryServiceHandle = *((uint16_t*)&pr->Attribute_Data_List[idx - 4]);
							hidApp.BatteryServiceEndHandle = *((uint16_t*)&pr->Attribute_Data_List[idx - 2]);
							hidApp.state = HidState::DiscoverCharacs;
							APP_DBG_MSG("-- GATT : Battery Service UUID: 0x%x\n", uuid);
						} else if (uuid == HUMAN_INTERFACE_DEVICE_SERVICE_UUID) {
							hidApp.HIDServiceHandle = *((uint16_t*)&pr->Attribute_Data_List[idx - 4]);
							hidApp.HIDServiceEndHandle = *((uint16_t*)&pr->Attribute_Data_List[idx - 2]);
							hidApp.state = HidState::DiscoverCharacs;
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

			if (hidApp.connHandle == pr->Connection_Handle) {
				// Event data: 2 bytes start handle | 1 byte char properties | 2 bytes handle | 2 or 16 bytes data
				// Only handle 16 bit UUIDs
				uint8_t idx = 5;
				if (pr->Handle_Value_Pair_Length == 7) {

					while (pr->Data_Length > 0) {
						uint16_t uuid = *((uint16_t*)&pr->Handle_Value_Pair_Data[idx]);
						// store the characteristic handle not the attribute handle
						uint16_t handle = *((uint16_t*)&pr->Handle_Value_Pair_Data[idx - 2]);

						APP_DBG_MSG("-- GATT : uuid %x handle: %x\n", uuid, handle);

						if (uuid == BATTERY_LEVEL_CHAR_UUID) {
							APP_DBG_MSG("-- GATT : Battery Notification Found\n");
							hidApp.state = HidState::DiscoverNotificationCharDesc;
							hidApp.BatteryNotificationCharHdle = handle;
						}
						if (uuid == REPORT_CHAR_UUID) {
							APP_DBG_MSG("-- GATT : HID Report Notification Found\n");
							hidApp.state = HidState::DiscoverNotificationCharDesc;
							hidApp.HIDNotificationCharHdle = handle;
						}
						pr->Data_Length -= 7;
						idx += 7;

					}
					// To avoid continually retriggering discover characteristics
					if (hidApp.state == HidState::DiscoverCharacs) {
						hidApp.state = HidState::DiscoveringCharacs;
					}
				}
			}
		}
		break;

		case ACI_ATT_FIND_INFO_RESP_VSEVT_CODE:
		{
			// After finding client configuration descriptor to activate notify event
			aci_att_find_info_resp_event_rp0 *pr = (aci_att_find_info_resp_event_rp0*)blecore_evt->data;

			if (hidApp.connHandle == pr->Connection_Handle) {
				uint8_t numDesc = (pr->Event_Data_Length) / 4;
				APP_DBG_MSG("-- GATT : Found %d client configuration descriptors\n", numDesc);

				// event data: 2 bytes handle | 2 bytes UUID
				uint8_t idx = 0;
				if (pr->Format == UUID_TYPE_16) {
					for (uint8_t i = 0; i < numDesc; i++) {
						uint16_t handle = *((uint16_t*)&pr->Handle_UUID_Pair[idx]);
						uint16_t uuid = *((uint16_t*)&pr->Handle_UUID_Pair[idx + 2]);

						if (uuid == CLIENT_CHAR_CONFIG_DESCRIPTOR_UUID && handle > hidApp.BatteryNotificationCharHdle && handle <= hidApp.BatteryServiceEndHandle) {
							APP_DBG_MSG("-- GATT : Battery Client Char Config Descriptor: 0x%x\n", handle);
							hidApp.BatteryNotificationDescHandle = handle;
							hidApp.state = HidState::EnableNotificationDesc;

						} else if (uuid == CLIENT_CHAR_CONFIG_DESCRIPTOR_UUID && handle > hidApp.HIDNotificationCharHdle && handle <= hidApp.HIDServiceEndHandle) {
							APP_DBG_MSG("-- GATT : HID Client Char Config Descriptor: 0x%x\n", handle);
							hidApp.HIDNotificationDescHandle = handle;
							hidApp.state = HidState::EnableNotificationDesc;
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

			if (hidApp.connHandle == pr->Connection_Handle) {

				if (pr->Attribute_Handle == hidApp.BatteryNotificationCharHdle) {
					hidApp.BatteryNotification(&pr->Attribute_Value[0], pr->Attribute_Value_Length);
				}
				if (pr->Attribute_Handle == hidApp.HIDNotificationCharHdle) {
					hidApp.HidNotification(&pr->Attribute_Value[0], pr->Attribute_Value_Length);
				}

			}
		}
		break;

		case ACI_GATT_PROC_COMPLETE_VSEVT_CODE:
		{
			aci_gatt_proc_complete_event_rp0 *pr = (aci_gatt_proc_complete_event_rp0*)blecore_evt->data;
			APP_DBG_MSG("-- GATT : ACI_GATT_PROC_COMPLETE_VSEVT_CODE \n\n");

			if (hidApp.connHandle == pr->Connection_Handle) {
				UTIL_SEQ_SetTask(1 << CFG_TASK_SEARCH_SERVICE_ID, CFG_SCH_PRIO_0);		// Triggers UpdateService()
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

	return SVCCTL_EvtNotAck;
}


//static inline int16_t Clamp(int16_t v, int16_t min, int16_t max)
//{
//	if (v > max)		return max;
//	else if (v < min) 	return min;
//	else 				return v;
//}


void HidApp::HidNotification(uint8_t* payload, uint8_t len)
{
	volatile int16_t* hidPayload = (int16_t*)payload;
	position3D.x = hidPayload[0];
	position3D.y = hidPayload[1];
	position3D.z = hidPayload[2];

	// Output to PWM - values vary from ~400 - 600
	//int16_t pwmX2 = std::clamp(position3D.x - 410, 0, 205) * 20;
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


void HidApp::BatteryNotification(uint8_t* payload, uint8_t len)
{
		APP_DBG_MSG(" -- P2P APPLICATION CLIENT: x: %d; y: %d; z: %d; Battery: %d%%\n", position3D.x, position3D.y, position3D.z, payload[0]);
}


void HidApp::UpdateService()
{
	// Triggered each time a GATT operation completes - keep track of discovery process status to get handles and activate notifications accordingly
	uint8_t enable = 1;
	uint8_t startHandle, endHandle;

	if (hidApp.state != HidState::Idle) {

		switch (hidApp.state)
		{

		case HidState::DiscoverServices:
			APP_DBG_MSG("P2P_DISCOVER_SERVICES\n");
			break;
		case HidState::DiscoverCharacs:
			APP_DBG_MSG("* GATT : Discover HID/Battery Characteristics\n");

			// Discover characteristics of both battery and HID services
			if (hidApp.BatteryServiceHandle > hidApp.HIDServiceHandle) {
				startHandle = hidApp.HIDServiceHandle;
				endHandle = hidApp.BatteryServiceEndHandle;
			} else {
				startHandle = hidApp.BatteryServiceHandle;
				endHandle = hidApp.HIDServiceEndHandle;
			}
			aci_gatt_disc_all_char_of_service(hidApp.connHandle, startHandle, endHandle);

			break;

		case HidState::DiscoverNotificationCharDesc:
			APP_DBG_MSG("* GATT : Discover HID/Battery Notification Characteristic Descriptor\n");

			if (hidApp.BatteryNotificationCharHdle > hidApp.HIDNotificationCharHdle) {
				startHandle = hidApp.HIDNotificationCharHdle;
				endHandle = hidApp.BatteryNotificationCharHdle + 2;
			} else {
				startHandle = hidApp.BatteryNotificationCharHdle;
				endHandle = hidApp.HIDNotificationCharHdle + 2;
			}

			aci_gatt_disc_all_char_desc(hidApp.connHandle, startHandle, endHandle);

			break;

		case HidState::EnableNotificationDesc:
			APP_DBG_MSG("* GATT : Enable Battery Notification\n");
			aci_gatt_write_char_desc(hidApp.connHandle, hidApp.BatteryNotificationDescHandle, 2, &enable);

			hidApp.state = HidState::EnableHIDNotificationDesc;
			break;

		case HidState::EnableHIDNotificationDesc:
			APP_DBG_MSG("* GATT : Enable HID Notification\n");
			aci_gatt_write_char_desc(hidApp.connHandle, hidApp.HIDNotificationDescHandle, 2, &enable);

			hidApp.state = HidState::ClientConnected;
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
}
