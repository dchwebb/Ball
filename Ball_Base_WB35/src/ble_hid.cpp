#include "app_common.h"
#include "ble.h"
#include "stm32_seq.h"
#include "app_ble.h"
#include "ble_hid.h"
#include "usb.h"
#include <Algorithm>

HidApp hidApp;

void HidApp::Init(void)
{
	UTIL_SEQ_RegTask(1 << CFG_TASK_HIDServiceDiscovery, UTIL_SEQ_RFU, HIDServiceDiscovery);
	UTIL_SEQ_RegTask(1 << CFG_TASK_GetBatteryLevel, UTIL_SEQ_RFU, GetBatteryLevel);
	state = HidState::Idle;
	SVCCTL_RegisterCltHandler(HIDEventHandler);
}


void HidApp::Calibrate()
{
	printf("Starting calibration\r\n");
	position3D.x = 2047.0f;
	position3D.y = 2047.0f;
	position3D.z = 2047.0f;
	calibX = 0.0f;
	calibY = 0.0f;
	calibZ = 0.0f;
	calibrateCounter = calibrateCount;
}


void HidApp::HidNotification(uint8_t* payload, uint8_t len)
{
	volatile int16_t* hidPayload = (int16_t*)payload;

	// If no offset configured perform calibration
	if (calibrateCounter == 0 && offsetX == 0 && offsetY == 0 && offsetZ == 0) {
		Calibrate();
	}

	if (calibrateCounter == 0) {
		// Raw data from remote is signed 16 bit integer
		position3D.x = std::clamp(((static_cast<float>(hidPayload[0]) - offsetX) / divider) + static_cast<float>(position3D.x), 0.0f, 4095.0f);
		position3D.y = std::clamp(((static_cast<float>(hidPayload[1]) - offsetY) / divider) + static_cast<float>(position3D.y), 0.0f, 4095.0f);
		position3D.z = std::clamp(((static_cast<float>(hidPayload[2]) - offsetZ) / divider) + static_cast<float>(position3D.z), 0.0f, 4095.0f);

	} else {
		calibX += hidPayload[0];
		calibY += hidPayload[1];
		calibZ += hidPayload[2];

		float div = (calibrateCount - calibrateCounter) + 1;

		if (calibrateCounter % 20 == 0) {
			printf("%ld: x: %.1f y: %.1f z: %.1f\r\n", (int32_t)div, calibX / div, calibY / div, calibZ / div);
		}

		if (--calibrateCounter == 0) {
			offsetX = calibX / calibrateCount;
			offsetY = calibY / calibrateCount;
			offsetZ = calibZ / calibrateCount;
			printf("New Offsets x: %.1f y: %.1f z: %.1f\r\n", offsetX, offsetY, offsetZ);
		}
	}


	if (outputGyro && (SysTickVal - lastPrint > 400)) {
		printf("x: %.1f y: %.1f z: %.1f; [received x: %d y: %d z: %d]\r\n", position3D.x, position3D.y, position3D.z, hidPayload[0], hidPayload[1], hidPayload[2]);
		lastPrint = SysTickVal;
	}

	TIM2->CCR1 = static_cast<int16_t>(position3D.x);
	TIM2->CCR2 = static_cast<int16_t>(position3D.y);
	TIM2->CCR3 = static_cast<int16_t>(position3D.z);
}


void HidApp::GetBatteryLevel()
{
	if (hidApp.state == HidApp::HidState::ClientConnected) {
		printf("* GATT : Get Battery Level\n");
		hidApp.action = HidApp::HidAction::BatteryLevel;
		aci_gatt_read_char_value(hidApp.connHandle, hidApp.BatteryNotificationCharHdle);
	}
}


void HidApp::HIDConnectionNotification()
{
	switch (bleApp.deviceConnectionStatus) {

	case BleApp::ConnectionStatus::ClientConnected:
		// Reset state of client
		state = HidState::DiscoverServices;
		BatteryNotificationCharHdle = 0;
		BatteryNotificationDescHandle = 0;
		BatteryServiceHandle = 0;
		HIDNotificationCharHdle = 0;
		HIDNotificationDescHandle = 0;
		HIDServiceHandle = 0;

		GPIOA->ODR |= GPIO_ODR_OD3;				// Turn on connection LED
		break;

	case BleApp::ConnectionStatus::Idle:
		state = HidState::Idle;
		action = HidAction::None;
		GPIOA->ODR &= ~GPIO_ODR_OD3;			// Turn off connection LED
		break;

	default:
		break;
	}
}


SVCCTL_EvtAckStatus_t HidApp::HIDEventHandler(void *Event)
{
	// Triggered from BleApp::UserEvtRx() - svc_ctl will try all registered event handlers to see which can deal with request
	SVCCTL_EvtAckStatus_t handled = SVCCTL_EvtNotAck;		// If event belongs to this handler return SVCCTL_EvtAckFlowEnable
	hci_event_pckt* hciEvent = (hci_event_pckt *)(((hci_uart_pckt*)Event)->data);

	switch (hciEvent->evt)
	{
	case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
	{
		auto* bleCoreEvent = (evt_blecore_aci*)hciEvent->data;
		switch (bleCoreEvent->ecode)
		{

		case ACI_ATT_READ_BY_GROUP_TYPE_RESP_VSEVT_CODE:

			if (hidApp.state == HidState::DiscoverServices) {
				// Triggered in app_ble: aci_gatt_disc_all_primary_services (initial HID discovery event)
				auto* attributeDataEvent = (aci_att_read_by_group_type_resp_event_rp0*)bleCoreEvent->data;

				hidApp.connHandle = attributeDataEvent->Connection_Handle;
				uint8_t numServ = (attributeDataEvent->Data_Length) / attributeDataEvent->Attribute_Data_Length;

				printf("* GATT : %d Services; Connection handle 0x%x \n", numServ, hidApp.connHandle);

				// Event data: 2 bytes start handle | 2 bytes end handle | 2 or 16 bytes data
				// Only if the UUID is 16 bit so check if the data length is 6
				if (attributeDataEvent->Attribute_Data_Length == 6) {
					uint8_t idx = 4;
					for (uint8_t i = 0; i < numServ; i++) {
						uint16_t uuid = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx]);

						if (uuid == BATTERY_SERVICE_UUID) {
							hidApp.BatteryServiceHandle = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx - 4]);
							hidApp.BatteryServiceEndHandle = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx - 2]);
							printf("  - Battery Service UUID: 0x%x\r\n", uuid);
						} else if (uuid == HUMAN_INTERFACE_DEVICE_SERVICE_UUID) {
							hidApp.HIDServiceHandle = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx - 4]);
							hidApp.HIDServiceEndHandle = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx - 2]);
							printf("  - HID Service UUID: 0x%x\r\n", uuid);
						} else {
							printf("  - Service UUID: 0x%x\r\n", uuid);
						}

						if (hidApp.BatteryServiceHandle > 0 || hidApp.HIDServiceHandle > 0){
							hidApp.state = HidState::DiscoverCharacteristics;
						}

						idx += 6;
					}
				}
				handled = SVCCTL_EvtAckFlowEnable;
			}
			break;

		case ACI_ATT_READ_BY_TYPE_RESP_VSEVT_CODE:

			if (hidApp.state == HidState::DiscoverCharacteristics || hidApp.state == HidState::DiscoveringCharacteristics || hidApp.state == HidState::DiscoveredCharacteristics) {
				// Triggered after discover characteristics
				auto* characteristicsEvent = (aci_att_read_by_type_resp_event_rp0*)bleCoreEvent->data;

				// Event data: 2 bytes start handle | 1 byte char properties | 2 bytes handle | 2 or 16 bytes data (Only handle 16 bit UUIDs)
				uint8_t idx = 5;
				if (characteristicsEvent->Handle_Value_Pair_Length == 7) {

					while (characteristicsEvent->Data_Length > 0) {
						uint16_t uuid = *((uint16_t*)&characteristicsEvent->Handle_Value_Pair_Data[idx]);
						// store the characteristic handle not the attribute handle
						uint16_t handle = *((uint16_t*)&characteristicsEvent->Handle_Value_Pair_Data[idx - 2]);

						switch (uuid) {
						case BATTERY_LEVEL_CHAR_UUID:
							printf("  - UUID: 0x%x handle: 0x%x [Battery Notification]\n", uuid, handle);
							hidApp.state = HidState::DiscoveredCharacteristics;
							hidApp.BatteryNotificationCharHdle = handle;
							break;
						case REPORT_CHAR_UUID:
							printf("  - UUID: 0x%x handle: 0x%x [HID Report Notification]\n", uuid, handle);
							hidApp.state = HidState::DiscoveredCharacteristics;
							hidApp.HIDNotificationCharHdle = handle;
							break;
						case REPORT_MAP_CHAR_UUID:
							printf("  - UUID: 0x%x handle: 0x%x [HID Report Map]\n", uuid, handle);
							hidApp.state = HidState::DiscoveredCharacteristics;
							hidApp.HIDReportMapHdle = handle;
							break;
						default:
							printf("  - UUID: 0x%x handle: 0x%x\r\n", uuid, handle);
						}

						characteristicsEvent->Data_Length -= 7;
						idx += 7;
					}
					// To avoid continually retriggering discover characteristics
					if (hidApp.state == HidState::DiscoverCharacteristics) {
						hidApp.state = HidState::DiscoveringCharacteristics;
					}
				}
				handled = SVCCTL_EvtAckFlowEnable;
			}
			break;

		case ACI_ATT_FIND_INFO_RESP_VSEVT_CODE:
			if (hidApp.state == HidState::DiscoveredCharacteristics || hidApp.state == HidState::EnableNotificationDesc  || hidApp.state == HidState::EnableHIDNotificationDesc) {
				// After finding client configuration descriptor to activate notify event
				auto *notificationEvent = (aci_att_find_info_resp_event_rp0*)bleCoreEvent->data;

				uint8_t numDesc = (notificationEvent->Event_Data_Length) / 4;
				printf("  - Found %d client configuration descriptors\r\n", numDesc);

				// event data: 2 bytes handle | 2 bytes UUID
				uint8_t idx = 0;
				if (notificationEvent->Format == UUID_TYPE_16) {
					for (uint8_t i = 0; i < numDesc; i++) {
						uint16_t handle = *((uint16_t*)&notificationEvent->Handle_UUID_Pair[idx]);
						uint16_t uuid = *((uint16_t*)&notificationEvent->Handle_UUID_Pair[idx + 2]);

						if (uuid == CLIENT_CHAR_CONFIG_DESCRIPTOR_UUID && handle > hidApp.BatteryNotificationCharHdle && handle <= hidApp.BatteryServiceEndHandle) {
							printf("  - Battery Client Char Config Descriptor: 0x%x\r\n", handle);
							hidApp.BatteryNotificationDescHandle = handle;
							hidApp.state = HidState::EnableNotificationDesc;

						} else if (uuid == CLIENT_CHAR_CONFIG_DESCRIPTOR_UUID && handle > hidApp.HIDNotificationCharHdle && handle <= hidApp.HIDServiceEndHandle) {
							printf("  - HID Client Char Config Descriptor: 0x%x\r\n", handle);
							hidApp.HIDNotificationDescHandle = handle;
							hidApp.state = HidState::EnableNotificationDesc;
						}
						idx += 4;
					}
				}
				handled = SVCCTL_EvtAckFlowEnable;
			}
			break;

		case ACI_ATT_READ_BLOB_RESP_VSEVT_CODE:
			if (hidApp.action == HidApp::HidAction::GetReportMap) {
				auto *pr = (aci_att_read_blob_resp_event_rp0*)bleCoreEvent->data;
				hidApp.PrintReportMap(pr->Attribute_Value, pr->Event_Data_Length);
				hidApp.state = HidState::Disconnect;
				handled = SVCCTL_EvtAckFlowEnable;
			}
			break;

		case ACI_ATT_READ_RESP_VSEVT_CODE:
			if (hidApp.action == HidApp::HidAction::BatteryLevel) {
				auto *pr = (aci_att_read_resp_event_rp0*)bleCoreEvent->data;
				printf("Battery Level: %d%%\n\r", pr->Attribute_Value[0]);
				hidApp.action = HidApp::HidAction::None;
				handled = SVCCTL_EvtAckFlowEnable;
			}
			break;

		case ACI_GATT_NOTIFICATION_VSEVT_CODE:
			{
				auto* pr = (aci_gatt_notification_event_rp0*)bleCoreEvent->data;

				if (pr->Attribute_Handle == hidApp.BatteryNotificationCharHdle) {
					hidApp.BatteryNotification(&pr->Attribute_Value[0], pr->Attribute_Value_Length);
					handled = SVCCTL_EvtAckFlowEnable;
				}
				if (pr->Attribute_Handle == hidApp.HIDNotificationCharHdle) {
					hidApp.HidNotification(&pr->Attribute_Value[0], pr->Attribute_Value_Length);
					handled = SVCCTL_EvtAckFlowEnable;
				}
			}
			break;

		case ACI_GATT_PROC_COMPLETE_VSEVT_CODE:
			if (hidApp.state != HidState::ClientConnected && hidApp.state != HidState::Idle) {
				auto* pr = (aci_gatt_proc_complete_event_rp0*)bleCoreEvent->data;
				if (hidApp.connHandle == pr->Connection_Handle) {
					UTIL_SEQ_SetTask(1 << CFG_TASK_HIDServiceDiscovery, CFG_SCH_PRIO_0);		// Call discovery state machine
				}
				handled = SVCCTL_EvtAckFlowEnable;
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

	return handled;
}


void HidApp::PrintReportMap(uint8_t* data, uint8_t len)
{
	usb.cdc.PrintString("* Report Map:\r\n%s\r\n", usb.cdc.HexToString(data, len, true));
}


void HidApp::BatteryNotification(uint8_t* payload, uint8_t len)
{
	printf(" -- Battery Notification: Battery: %d%%\n", payload[0]);
}


void HidApp::GetReportMap()
{
	printf("* GATT : Read Report Map\n");
	//aci_gatt_read_char_value(hidApp.connHandle, hidApp.HIDReportMapHdle);
	aci_gatt_read_long_char_value(hidApp.connHandle, hidApp.HIDReportMapHdle, 0);
}


void HidApp::HIDServiceDiscovery()
{
	// Triggered each time a GATT operation completes - keep track of discovery process status to get handles and activate notifications accordingly
	uint8_t enable = 1;
	uint8_t startHandle, endHandle;

	if (hidApp.state != HidState::Idle) {

		switch (hidApp.state)
		{

		case HidState::DiscoverCharacteristics:
			printf("* GATT : Discover HID/Battery Characteristics\n");

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

		case HidState::DiscoveredCharacteristics:

			if (hidApp.action == HidAction::GetReportMap) {
				printf("* GATT : Read Report Map\n");
				aci_gatt_read_long_char_value(hidApp.connHandle, hidApp.HIDReportMapHdle, 0);

			} else {

				printf("* GATT : Discover HID/Battery Notification Characteristic Descriptor\n");

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
			printf("* GATT : Enable Battery Notification\n");
			aci_gatt_write_char_desc(hidApp.connHandle, hidApp.BatteryNotificationDescHandle, 2, &enable);

			hidApp.state = HidState::EnableHIDNotificationDesc;
			break;

		case HidState::EnableHIDNotificationDesc:
			printf("* GATT : Enable HID Notification\n");
			aci_gatt_write_char_desc(hidApp.connHandle, hidApp.HIDNotificationDescHandle, 2, &enable);

			hidApp.state = HidState::ClientConnected;
			break;

		case HidState::Disconnect:
			printf("* GATT : Disconnect\n");
			bleApp.DisconnectRequest();

			break;

		default:
			break;
		}
	}
}
