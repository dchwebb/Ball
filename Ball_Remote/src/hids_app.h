#pragma once

#include "svc_ctl.h"

typedef enum  {
	ReportMap,
	HidInformation,
	ReportJoystick,
} characteristics_t;

typedef enum
{
  HIDS_REPORT_NOTIFICATION_ENABLED,
  HIDS_REPORT_NOTIFICATION_DISABLED,
  HIDS_KEYB_INPUT_NOTIFY_ENABLED,
  HIDS_KEYB_INPUT_NOTIFY_DISABLED,
  HIDS_MOUSE_INPUT_NOTIFY_ENABLED,
  HIDS_MOUSE_INPUT_NOTIFY_DISABLED,
  HIDS_OUTPUT_REPORT,
  HIDS_KEYBOARD_INPUT_REPORT,
  HIDS_KEYBOARD_OUTPUT_REPORT,
  HIDS_MOUSE_INPUT_REPORT,
  HIDS_CONTROL_WRITE,
  HIDS_CONN_HANDLE_EVT,
  HIDS_DISCON_HANDLE_EVT
} HIDS_Opcode_Notification_evt_t;

typedef struct {
  HIDS_Opcode_Notification_evt_t  HIDS_Evt_Opcode;
  uint8_t   Instance;
  uint8_t   Index;
  uint8_t   ReportLength;
  uint8_t   *pReport;
} HIDS_Notification_evt_t;

struct HidService
{
public:
	void Init();
	void AppInit();
	void JoystickNotification(uint16_t x, uint16_t y, uint16_t z);
	static SVCCTL_EvtAckStatus_t HIDS_Event_Handler(void *Event);
private:
	enum HIDInfoFlags {RemoteWake = 1, NormallyConnectable = 2};
	static constexpr uint16_t JoystickReportID  = 0x0101;	// First Byte: Input Report (0x01) | Second Byte: Report ID (0x1 ... 0x03)

	const struct HIDInformation {
		uint16_t bcdHID = 0x0101;			// Binary coded decimal HID version
		uint8_t  bcountryCode = 0;  		// 0 = not localized
		uint8_t  flags = RemoteWake | NormallyConnectable;          // See http://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.hid_information.xml
	} hidInformation;

	uint16_t ServiceHandle;				 	// Service handle
	uint16_t ReportJoystickHandle;
	uint16_t ReportJoystickRefDescHandle;
	uint16_t HidInformationHandle;
	uint16_t HidControlPointHdle;
	uint16_t ReportMapHandle;
	bool JoystickNotifications;


	tBleStatus UpdateChar(characteristics_t characteristic);

};

extern "C" {		// Declare with C linkage or will be overridden by weak declaration in svc_ctl.c
void HIDS_Init();
}

tBleStatus HIDS_Update_Char(characteristics_t characteristic);
void HIDS_Joystick_Notification(uint16_t x, uint16_t y, uint16_t z);
void HIDS_App_Init();

extern HidService hidService;
