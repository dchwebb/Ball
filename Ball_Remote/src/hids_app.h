#pragma once

typedef enum  {
	ReportMap,
//	BootMouseInput,
	HidInformation,
	ReportJoystick,
//	ReportMPlayer,
//	ReportButtons,
//	ProtocolMode
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




//void HIDS_Init(void);
tBleStatus HIDS_Update_Char(characteristics_t characteristic, uint8_t Report_Index, uint8_t report_size, uint8_t *pPayload);
void HIDS_Joystick_Notification(uint16_t x, uint16_t y, uint16_t z);
extern "C" {
void HIDS_Init();
}
void HIDS_App_Init();
