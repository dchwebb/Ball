#include "main.h"
#include "app_common.h"
#include "dbg_trace.h"
#include "ble.h"
#include "stm32_seq.h"
#include "app_ble.h"
#include "ble_hid.h"
#include "usb.h"
#include <Algorithm>

HidApp hidApp;
extern USBHandler usb;

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

	case BleApp::ConnectionStatus::ClientConnected:
		// Reset state of client
		BatteryNotificationCharHdle = 0;
		BatteryNotificationDescHandle = 0;
		BatteryServiceHandle = 0;
		HIDNotificationCharHdle = 0;
		HIDNotificationDescHandle = 0;
		HIDServiceHandle = 0;

		HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
		break;

	case BleApp::ConnectionStatus::Idle:
		state = HidState::Idle;
		action = HidAction::None;
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
				BleApp::ConnectionStatus status = bleApp.GetClientConnectionStatus(hidApp.connHandle);

				// Handle disconnected
				if ((hidApp.state == HidState::ClientConnected) && (status == BleApp::ConnectionStatus::Idle)) {
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
							APP_DBG_MSG("  - Battery Service UUID: 0x%x\n", uuid);
						} else if (uuid == HUMAN_INTERFACE_DEVICE_SERVICE_UUID) {
							hidApp.HIDServiceHandle = *((uint16_t*)&pr->Attribute_Data_List[idx - 4]);
							hidApp.HIDServiceEndHandle = *((uint16_t*)&pr->Attribute_Data_List[idx - 2]);
							hidApp.state = HidState::DiscoverCharacs;
							APP_DBG_MSG("  - HID Service UUID: 0x%x\n", uuid);
						} else {
							APP_DBG_MSG("  - Service UUID: 0x%x\n", uuid);
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
				// Event data: 2 bytes start handle | 1 byte char properties | 2 bytes handle | 2 or 16 bytes data (Only handle 16 bit UUIDs)
				uint8_t idx = 5;
				if (pr->Handle_Value_Pair_Length == 7) {

					while (pr->Data_Length > 0) {
						uint16_t uuid = *((uint16_t*)&pr->Handle_Value_Pair_Data[idx]);
						// store the characteristic handle not the attribute handle
						uint16_t handle = *((uint16_t*)&pr->Handle_Value_Pair_Data[idx - 2]);

						switch (uuid) {
						case BATTERY_LEVEL_CHAR_UUID:
							APP_DBG_MSG("  - uuid %x handle: %x [Battery Notification]\n", uuid, handle);
							hidApp.state = HidState::DiscoveredCharacs;
							hidApp.BatteryNotificationCharHdle = handle;
							break;
						case REPORT_CHAR_UUID:
							APP_DBG_MSG("  - uuid %x handle: %x [HID Report Notification\n", uuid, handle);
							hidApp.state = HidState::DiscoveredCharacs;
							hidApp.HIDNotificationCharHdle = handle;
							break;
						case REPORT_MAP_CHAR_UUID:
							APP_DBG_MSG("  - uuid %x handle: %x [HID Report Map]\n", uuid, handle);
							hidApp.state = HidState::DiscoveredCharacs;
							hidApp.HIDReportMapHdle = handle;
							break;
						default:
							APP_DBG_MSG("  - uuid %x handle: %x\n", uuid, handle);
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
				APP_DBG_MSG("  - Found %d client configuration descriptors\n", numDesc);

				// event data: 2 bytes handle | 2 bytes UUID
				uint8_t idx = 0;
				if (pr->Format == UUID_TYPE_16) {
					for (uint8_t i = 0; i < numDesc; i++) {
						uint16_t handle = *((uint16_t*)&pr->Handle_UUID_Pair[idx]);
						uint16_t uuid = *((uint16_t*)&pr->Handle_UUID_Pair[idx + 2]);

						if (uuid == CLIENT_CHAR_CONFIG_DESCRIPTOR_UUID && handle > hidApp.BatteryNotificationCharHdle && handle <= hidApp.BatteryServiceEndHandle) {
							APP_DBG_MSG("  - Battery Client Char Config Descriptor: 0x%x\n", handle);
							hidApp.BatteryNotificationDescHandle = handle;
							hidApp.state = HidState::EnableNotificationDesc;

						} else if (uuid == CLIENT_CHAR_CONFIG_DESCRIPTOR_UUID && handle > hidApp.HIDNotificationCharHdle && handle <= hidApp.HIDServiceEndHandle) {
							APP_DBG_MSG("  - HID Client Char Config Descriptor: 0x%x\n", handle);
							hidApp.HIDNotificationDescHandle = handle;
							hidApp.state = HidState::EnableNotificationDesc;
						}

						idx += 4;
					}
				}
			}
		}
		break;

		case ACI_ATT_READ_BLOB_RESP_VSEVT_CODE:
		{
			aci_att_read_blob_resp_event_rp0 *pr = (aci_att_read_blob_resp_event_rp0*)blecore_evt->data;
			hidApp.PrintReportMap(pr->Attribute_Value, pr->Event_Data_Length);
			hidApp.state = HidState::Disconnect;
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

std::string rptMap;
void HidApp::PrintReportMap(uint8_t* data, uint8_t len)
{
	rptMap = "* Report Map:\r\n" + HexToString(data, len, true) + "\r\n";
	usb.SendString(rptMap);
}


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

		case HidState::DiscoveredCharacs:

			if (hidApp.action == HidAction::GetReportMap) {
				APP_DBG_MSG("* GATT : Read Report Map\n");
				//aci_gatt_read_char_value(hidApp.connHandle, hidApp.HIDReportMapHdle);
				aci_gatt_read_long_char_value(hidApp.connHandle, hidApp.HIDReportMapHdle, 0);

			} else {

				APP_DBG_MSG("* GATT : Discover HID/Battery Notification Characteristic Descriptor\n");

				if (hidApp.BatteryNotificationCharHdle > hidApp.HIDNotificationCharHdle) {
					startHandle = hidApp.HIDNotificationCharHdle;
					endHandle = hidApp.BatteryNotificationCharHdle + 2;
				} else {
					startHandle = hidApp.BatteryNotificationCharHdle;
					endHandle = hidApp.HIDNotificationCharHdle + 2;
				}

				aci_gatt_disc_all_char_desc(hidApp.connHandle, startHandle, endHandle);
			}
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

		case HidState::Disconnect:
			APP_DBG_MSG("* GATT : Disconnect\n");
			bleApp.DisconnectRequest();

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
