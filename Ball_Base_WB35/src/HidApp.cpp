#include <BleApp.h>
#include <HidApp.h>
#include "app_common.h"
#include "ble.h"
#include "stm32_seq.h"
#include "usb.h"
#include <Algorithm>

HidApp hidApp;

void HidApp::Init(void)
{
	UTIL_SEQ_RegTask(1 << CFG_TASK_HIDServiceDiscovery, UTIL_SEQ_RFU, HIDServiceDiscovery);
	UTIL_SEQ_RegTask(1 << CFG_TASK_GetBatteryLevel, UTIL_SEQ_RFU, GetBatteryLevel);
	UTIL_SEQ_RegTask(1 << CFG_TASK_GyroCommand, UTIL_SEQ_RFU, GyroCommand);
	UTIL_SEQ_RegTask(1 << CFG_TASK_ReadGyroRegister, UTIL_SEQ_RFU, ReadGyroRegister);
	state = HidState::Idle;
	SVCCTL_RegisterCltHandler(HIDEventHandler);
}


void HidApp::HidNotification(int16_t* payload, uint8_t len)
{
	// Convert raw HID data from signed 16 bit int to float
	Pos3D hidData {(float)(settings.invertX * payload[0]), (float)(settings.invertY * payload[1]), (float)(settings.invertZ * payload[2])};

	// Scale down and clamp to 12 bit value
	position3D.x = std::clamp((hidData.x * settings.scaleMult) + position3D.x, 0.0f, 4095.0f);
	position3D.y = std::clamp((hidData.y * settings.scaleMult) + position3D.y, 0.0f, 4095.0f);
	position3D.z = std::clamp((hidData.z * settings.scaleMult) + position3D.z, 0.0f, 4095.0f);


	if (outputGyro && (SysTickVal - lastPrint > 400)) {
		printf("Pos: %.1f/%.1f/%.1f;  Gyro: %.1f/%.1f/%.1f \r\n", position3D.x, position3D.y, position3D.z, hidData.x, hidData.y, hidData.z);
		lastPrint = SysTickVal;
	}

	TIM2->CCR1 = static_cast<int16_t>(position3D.x);
	TIM2->CCR2 = static_cast<int16_t>(position3D.y);
	TIM2->CCR3 = static_cast<int16_t>(position3D.z);

}


void HidApp::GyroCommand()
{
	// Set the gyroscope command (register number for read, register and value for write)
	if (hidApp.state == HidApp::HidState::ClientConnected) {
		aci_gatt_write_char_value(hidApp.connHandle,
				hidApp.gyroCmdNotificationCharHandle,
				(uint8_t)hidApp.gyroCmdType,
				(uint8_t*)&hidApp.gyroCommand);

		if (hidApp.gyroCmdType == GyroCmdType::read) {
			UTIL_SEQ_SetTask(1 << CFG_TASK_ReadGyroRegister, CFG_SCH_PRIO_0);
		}
	}
}


void HidApp::ReadGyroRegister()
{
	// Set the gyroscope register number for subsequent read
	if (hidApp.state == HidApp::HidState::ClientConnected) {
		hidApp.action = HidApp::HidAction::GyroRead;
		tBleStatus result = 0x0C;
		uint32_t retryCount = 0;

		// this command returns error 0x0C which is undocumented but clears after a while, so probably a 'not ready' error
		while (result != BLE_STATUS_SUCCESS && retryCount < 1000) {
			result = aci_gatt_read_char_value(hidApp.connHandle, hidApp.gyroRegNotificationCharHandle);
			++retryCount;
		}
	}
}


void HidApp::HIDConnectionNotification()
{
	switch (bleApp.connectionStatus) {

	case BleApp::ConnectionStatus::ClientConnected:
		// Reset state of client
		state = HidState::DiscoverServices;
		batteryNotificationCharHandle = 0;
		batteryNotificationDescHandle = 0;
		batteryServiceHandle = 0;
		hidNotificationCharHandle = 0;
		hidNotificationDescHandle = 0;
		hidServiceHandle = 0;

		batteryLevel = 0;
		break;

	case BleApp::ConnectionStatus::Idle:
		state = HidState::Idle;
		action = HidAction::None;
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
							hidApp.batteryServiceHandle = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx - 4]);
							hidApp.batteryServiceEndHandle = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx - 2]);
							printf("  - Battery Service UUID: 0x%x\r\n", uuid);
						} else if (uuid == HUMAN_INTERFACE_DEVICE_SERVICE_UUID) {
							hidApp.hidServiceHandle = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx - 4]);
							hidApp.hidServiceEndHandle = *((uint16_t*)&attributeDataEvent->Attribute_Data_List[idx - 2]);
							printf("  - HID Service UUID: 0x%x\r\n", uuid);
						} else {
							printf("  - Service UUID: 0x%x\r\n", uuid);
						}

						if (hidApp.batteryServiceHandle > 0 || hidApp.hidServiceHandle > 0){
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
							hidApp.batteryNotificationCharHandle = handle;
							break;
						case REPORT_CHAR_UUID:
							printf("  - UUID: 0x%x handle: 0x%x [HID Report Notification]\n", uuid, handle);
							hidApp.state = HidState::DiscoveredCharacteristics;
							hidApp.hidNotificationCharHandle = handle;
							break;
						case REPORT_MAP_CHAR_UUID:
							printf("  - UUID: 0x%x handle: 0x%x [HID Report Map]\n", uuid, handle);
							hidApp.state = HidState::DiscoveredCharacteristics;
							hidApp.hidReportMapHandle = handle;
							break;
						case GYRO_CMD_CHAR_UUID:
							printf("  - UUID: 0x%x handle: 0x%x [Gyro Command]\n", uuid, handle);
							hidApp.state = HidState::DiscoveredCharacteristics;
							hidApp.gyroCmdNotificationCharHandle = handle;
							break;
						case GYRO_REG_CHAR_UUID:
							printf("  - UUID: 0x%x handle: 0x%x [Gyro Register]\n", uuid, handle);
							hidApp.state = HidState::DiscoveredCharacteristics;
							hidApp.gyroRegNotificationCharHandle = handle;
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
			if (hidApp.state == HidState::DiscoveredCharacteristics || hidApp.state == HidState::EnableNotificationDesc) {
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

						if (uuid == CLIENT_CHAR_CONFIG_DESCRIPTOR_UUID) {
							if (handle > hidApp.batteryNotificationCharHandle && handle <= hidApp.batteryServiceEndHandle) {
								printf("  - Battery Client Char Config Descriptor: 0x%x\r\n", handle);
								hidApp.batteryNotificationDescHandle = handle;

							} else if (handle > hidApp.hidNotificationCharHandle && handle <= hidApp.hidServiceEndHandle) {
								printf("  - HID Client Char Config Descriptor: 0x%x\r\n", handle);
								hidApp.hidNotificationDescHandle = handle;

							} else if (handle > hidApp.gyroCmdNotificationCharHandle && handle <= hidApp.hidServiceEndHandle) {
								printf("  - Gyro Cmd Client Char Config Descriptor: 0x%x\r\n", handle);
								hidApp.gyroCmdNotificationDescHandle = handle;

							} else if (handle > hidApp.gyroRegNotificationCharHandle && handle <= hidApp.hidServiceEndHandle) {
								printf("  - Gyro Reg Client Char Config Descriptor: 0x%x\r\n", handle);
								hidApp.gyroRegNotificationDescHandle = handle;

							}
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
			{
				auto *pr = (aci_att_read_resp_event_rp0*)bleCoreEvent->data;

				if (hidApp.action == HidApp::HidAction::BatteryLevel) {
					printf("Battery Level: %d%%\r\n", pr->Attribute_Value[0]);
					hidApp.action = HidApp::HidAction::None;
					handled = SVCCTL_EvtAckFlowEnable;
				}
				if (hidApp.action == HidApp::HidAction::GyroRead) {
					printf("Gyroscope Value: %#02x\r\n", pr->Attribute_Value[0]);
					hidApp.action = HidApp::HidAction::None;
					handled = SVCCTL_EvtAckFlowEnable;
				}
			}
			break;

		case ACI_GATT_NOTIFICATION_VSEVT_CODE:
			{
				auto* pr = (aci_gatt_notification_event_rp0*)bleCoreEvent->data;

				if (pr->Attribute_Handle == hidApp.batteryNotificationCharHandle) {
					hidApp.BatteryNotification(&pr->Attribute_Value[0], pr->Attribute_Value_Length);
					handled = SVCCTL_EvtAckFlowEnable;
				}
				if (pr->Attribute_Handle == hidApp.hidNotificationCharHandle) {
					hidApp.HidNotification((int16_t*)&pr->Attribute_Value[0], pr->Attribute_Value_Length);
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


void HidApp::GetBatteryLevel()
{
	if (hidApp.state == HidApp::HidState::ClientConnected) {
		printf("Get Battery Level\n");
		hidApp.lastBatteryCheck = SysTickVal;
		hidApp.action = HidApp::HidAction::BatteryLevel;
		aci_gatt_read_char_value(hidApp.connHandle, hidApp.batteryNotificationCharHandle);
	}
}

void HidApp::BatteryNotification(uint8_t* payload, uint8_t len)
{
	batteryLevel = payload[0];

	if (outputGyro) {
		printf("* Battery Notification: Battery: %d%%\n", batteryLevel);
	}
}


void HidApp::CheckBattery()
{
	// Run periodically to check battery level while Remote is sleeping (only start checking after first battery notification received)
	if (hidApp.state == HidApp::HidState::ClientConnected && batteryLevel > 0 && settings.batteryInterval > 0
			&& SysTickVal - lastBatteryCheck > (settings.batteryInterval * 1000)) {
		GetBatteryLevel();
	}
}


void HidApp::PrintReportMap(uint8_t* data, uint8_t len)
{
	usb.cdc.PrintString("* Report Map:\r\n%s\r\n", usb.cdc.HexToString(data, len, true));
}


void HidApp::GetReportMap()
{
	printf("* GATT : Read Report Map\n");
	aci_gatt_read_long_char_value(hidApp.connHandle, hidApp.hidReportMapHandle, 0);
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
			if (hidApp.batteryServiceHandle > hidApp.hidServiceHandle) {
				startHandle = hidApp.hidServiceHandle;
				endHandle = hidApp.batteryServiceEndHandle;
			} else {
				startHandle = hidApp.batteryServiceHandle;
				endHandle = hidApp.hidServiceEndHandle;
			}
			aci_gatt_disc_all_char_of_service(hidApp.connHandle, startHandle, endHandle);

			break;

		case HidState::DiscoveredCharacteristics:

			if (hidApp.action == HidAction::GetReportMap) {
				printf("* GATT : Read Report Map\n");
				aci_gatt_read_long_char_value(hidApp.connHandle, hidApp.hidReportMapHandle, 0);

			} else {

				printf("* GATT : Discover HID/Battery Notification Characteristic Descriptor\n");

				if (hidApp.batteryNotificationCharHandle > hidApp.hidNotificationCharHandle) {
					startHandle = hidApp.hidNotificationCharHandle;
					endHandle = hidApp.batteryNotificationCharHandle + 2;
				} else {
					startHandle = hidApp.batteryNotificationCharHandle;
					endHandle = hidApp.hidNotificationCharHandle + 2;
				}

				aci_gatt_disc_all_char_desc(hidApp.connHandle, startHandle, endHandle);
			}
			break;

		case HidState::EnableNotificationDesc:
			printf("* GATT : Enable Battery Notification\n");
			aci_gatt_write_char_desc(hidApp.connHandle, hidApp.batteryNotificationDescHandle, 2, &enable);
			hidApp.state = HidState::EnableHIDNotificationDesc;
			break;

		case HidState::EnableHIDNotificationDesc:
			printf("* GATT : Enable HID Notification\n");
			aci_gatt_write_char_desc(hidApp.connHandle, hidApp.hidNotificationDescHandle, 2, &enable);
			hidApp.state = HidState::ClientConnected;
			hidApp.action = HidAction::None;
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



uint32_t HidApp::SerialiseConfig(uint8_t** buff)
{
	*buff = reinterpret_cast<uint8_t*>(&settings);
	return sizeof(settings);
}


uint32_t HidApp::StoreConfig(uint8_t* buff)
{
	if (buff != nullptr) {
		memcpy(&settings, buff, sizeof(settings));
	}

	return sizeof(settings);
}
