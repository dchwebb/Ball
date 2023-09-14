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
	state = HidState::Idle;
	SVCCTL_RegisterCltHandler(HIDEventHandler);
}


void HidApp::Calibrate()
{
	position3D.x = 2047;
	position3D.y = 2047;
	position3D.z = 2047;
	calibX = 0;
	calibY = 0;
	calibZ = 0;
	calibrateCounter = calibrateCount;
}


void HidApp::HidNotification(uint8_t* payload, uint8_t len)
{
	volatile int16_t* hidPayload = (int16_t*)payload;

	static uint32_t lastPrint = 0;
	if (outputGyro && (SysTickVal - lastPrint > 400)) {
		printf("x: %d y: %d z: %d\r\n", hidPayload[0], hidPayload[1], hidPayload[2]);
		lastPrint = SysTickVal;
	}

	if (calibrateCounter > 0) {

		calibX += hidPayload[0];
		calibY += hidPayload[1];
		calibZ += hidPayload[2];

		float div = (calibrateCount - calibrateCounter) + 1;

		printf("%ld: x: %3.3f y: %3.3f z: %3.3f\r\n", (int32_t)div, calibX / div, calibY / div, calibZ / div);

		--calibrateCounter;

		if (calibrateCounter == 0) {
			offsetX = calibX / calibrateCount;
			offsetY = calibY / calibrateCount;
			offsetZ = calibZ / calibrateCount;
			printf("New Offsets x: %3.3f y: %3.3f z: %d\r\n", offsetX, offsetY, static_cast<int16_t>(offsetZ));
		}
	} else {
		// Raw data from remote is signed 16 bit integer
		float newX = std::clamp(((static_cast<float>(hidPayload[0]) - offsetX) / divider) + static_cast<float>(position3D.x), 0.0f, 4095.0f);
		float newY = std::clamp(((static_cast<float>(hidPayload[1]) - offsetY) / divider) + static_cast<float>(position3D.y), 0.0f, 4095.0f);
		float newZ = std::clamp(((static_cast<float>(hidPayload[2]) - offsetZ) / divider) + static_cast<float>(position3D.z), 0.0f, 4095.0f);

		position3D.x = static_cast<int16_t>(newX);
		position3D.y = static_cast<int16_t>(newY);
		position3D.z = static_cast<int16_t>(newZ);
	}

	TIM2->CCR1 = position3D.x;
	TIM2->CCR2 = position3D.y;
	TIM2->CCR3 = position3D.z;
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
	hci_event_pckt* hciEvent = (hci_event_pckt *)(((hci_uart_pckt*)Event)->data);

	switch (hciEvent->evt)
	{
	case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
	{
		auto* bleCoreEvent = (evt_blecore_aci*)hciEvent->data;
		switch (bleCoreEvent->ecode)
		{

		case ACI_ATT_READ_BY_GROUP_TYPE_RESP_VSEVT_CODE:
		{
			// Triggered in app_ble: aci_gatt_disc_all_primary_services (initial HID discovery event)
			auto* attributeDataEvent = (aci_att_read_by_group_type_resp_event_rp0*)bleCoreEvent->data;

			hidApp.connHandle = attributeDataEvent->Connection_Handle;
			uint8_t numServ = (attributeDataEvent->Data_Length) / attributeDataEvent->Attribute_Data_Length;

			printf("-- GATT : Services: %d; Connection handle 0x%x \n", numServ, hidApp.connHandle);

			// Event data: 2 bytes start handle | 2 bytes end handle | 2 or 16 bytes data
			// Only if the UUID is 16 bit so check if the data length is 6
			if (attributeDataEvent->Attribute_Data_Length == 6) {
				std::string servicetext;
				uint8_t idx = 4;
				for (uint8_t i = 0; i < numServ; i++) {
					uint16_t uuid = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx]);

					servicetext+= "  - Service UUID: 0x" + HexToString((uint32_t)uuid, false);
					if (uuid == BATTERY_SERVICE_UUID) {
						hidApp.BatteryServiceHandle = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx - 4]);
						hidApp.BatteryServiceEndHandle = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx - 2]);
						//printf("  - Battery Service UUID: 0x%x\n", uuid);
						servicetext += " [Battery]\n";
					} else if (uuid == HUMAN_INTERFACE_DEVICE_SERVICE_UUID) {
						hidApp.HIDServiceHandle = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx - 4]);
						hidApp.HIDServiceEndHandle = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx - 2]);
						//printf("  - HID Service UUID: 0x%x\n", uuid);
						servicetext += " [HID]\n";
					} else {
						//printf("  - Service UUID: 0x%x\n", uuid);
						servicetext += "\n";
					}

					if (hidApp.BatteryServiceHandle > 0 || hidApp.HIDServiceHandle > 0){
						hidApp.state = HidState::DiscoverCharacteristics;
					}

					idx += 6;
				}
				usb.SendString(servicetext);
			}
		}
		break;

		case ACI_ATT_READ_BY_TYPE_RESP_VSEVT_CODE:
		{
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
						printf("  - uuid %x handle: %x [Battery Notification]\n", uuid, handle);
						hidApp.state = HidState::DiscoveredCharacteristics;
						hidApp.BatteryNotificationCharHdle = handle;
						break;
					case REPORT_CHAR_UUID:
						printf("  - uuid %x handle: %x [HID Report Notification\n", uuid, handle);
						hidApp.state = HidState::DiscoveredCharacteristics;
						hidApp.HIDNotificationCharHdle = handle;
						break;
					case REPORT_MAP_CHAR_UUID:
						printf("  - uuid %x handle: %x [HID Report Map]\n", uuid, handle);
						hidApp.state = HidState::DiscoveredCharacteristics;
						hidApp.HIDReportMapHdle = handle;
						break;
					default:
						printf("  - uuid %x handle: %x\n", uuid, handle);
					}

					characteristicsEvent->Data_Length -= 7;
					idx += 7;
				}
				// To avoid continually retriggering discover characteristics
				if (hidApp.state == HidState::DiscoverCharacteristics) {
					hidApp.state = HidState::DiscoveringCharacteristics;
				}
			}
		}
		break;

		case ACI_ATT_FIND_INFO_RESP_VSEVT_CODE:
		{
			// After finding client configuration descriptor to activate notify event
			auto *notificationEvent = (aci_att_find_info_resp_event_rp0*)bleCoreEvent->data;

			uint8_t numDesc = (notificationEvent->Event_Data_Length) / 4;
			printf("  - Found %d client configuration descriptors\n", numDesc);

			// event data: 2 bytes handle | 2 bytes UUID
			uint8_t idx = 0;
			if (notificationEvent->Format == UUID_TYPE_16) {
				for (uint8_t i = 0; i < numDesc; i++) {
					uint16_t handle = *((uint16_t*)&notificationEvent->Handle_UUID_Pair[idx]);
					uint16_t uuid = *((uint16_t*)&notificationEvent->Handle_UUID_Pair[idx + 2]);

					if (uuid == CLIENT_CHAR_CONFIG_DESCRIPTOR_UUID && handle > hidApp.BatteryNotificationCharHdle && handle <= hidApp.BatteryServiceEndHandle) {
						printf("  - Battery Client Char Config Descriptor: 0x%x\n", handle);
						hidApp.BatteryNotificationDescHandle = handle;
						hidApp.state = HidState::EnableNotificationDesc;

					} else if (uuid == CLIENT_CHAR_CONFIG_DESCRIPTOR_UUID && handle > hidApp.HIDNotificationCharHdle && handle <= hidApp.HIDServiceEndHandle) {
						printf("  - HID Client Char Config Descriptor: 0x%x\n", handle);
						hidApp.HIDNotificationDescHandle = handle;
						hidApp.state = HidState::EnableNotificationDesc;
					}

					idx += 4;
				}
			}
		}
		break;

		case ACI_ATT_READ_BLOB_RESP_VSEVT_CODE:
		{
			auto *pr = (aci_att_read_blob_resp_event_rp0*)bleCoreEvent->data;
			hidApp.PrintReportMap(pr->Attribute_Value, pr->Event_Data_Length);
			hidApp.state = HidState::Disconnect;
		}
		break;

		case ACI_GATT_NOTIFICATION_VSEVT_CODE:
		{
			auto* pr = (aci_gatt_notification_event_rp0*)bleCoreEvent->data;

			if (pr->Attribute_Handle == hidApp.BatteryNotificationCharHdle) {
				hidApp.BatteryNotification(&pr->Attribute_Value[0], pr->Attribute_Value_Length);
			}
			if (pr->Attribute_Handle == hidApp.HIDNotificationCharHdle) {
				hidApp.HidNotification(&pr->Attribute_Value[0], pr->Attribute_Value_Length);
			}
		}
		break;

		case ACI_GATT_PROC_COMPLETE_VSEVT_CODE:
		{
			auto* pr = (aci_gatt_proc_complete_event_rp0*)bleCoreEvent->data;

			if (hidApp.connHandle == pr->Connection_Handle) {
				UTIL_SEQ_SetTask(1 << CFG_TASK_HIDServiceDiscovery, CFG_SCH_PRIO_0);		// Call discovery state machine
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


void HidApp::PrintReportMap(uint8_t* data, uint8_t len)
{

	usb.SendString("* Report Map:\r\n" + HexToString(data, len, true) + "\r\n");
}


void HidApp::BatteryNotification(uint8_t* payload, uint8_t len)
{
		printf(" -- Battery Notification: x: %d; y: %d; z: %d; Battery: %d%%\n", position3D.x, position3D.y, position3D.z, payload[0]);
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
				//aci_gatt_read_char_value(hidApp.connHandle, hidApp.HIDReportMapHdle);
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
