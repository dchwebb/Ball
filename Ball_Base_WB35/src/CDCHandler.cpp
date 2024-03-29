#include "USB.h"
#include "CDCHandler.h"
#include <charconv>
extern "C" {
#include "shci.h"
#include "stm32_seq.h"
#include "ble_hci_le.h"
}
#include <BleApp.h>
#include <HidApp.h>
#include "configManager.h"

// Check if a command has been received from USB, parse and action as required
void CDCHandler::ProcessCommand()
{
	if (!cmdPending) {
		return;
	}

	const std::string_view cmd {comCmd};

	// Provide option to switch to USB DFU mode - this allows the MCU to be programmed with STM32CubeProgrammer in DFU mode
	if (cmd.compare("info") == 0) {		// Print diagnostic information

		int8_t rssi = -127;
		if (bleApp.connectionStatus == BleApp::ConnectionStatus::ClientConnected) {
			hci_read_rssi(bleApp.connectionHandle, (uint8_t*)&rssi);
		}

		WirelessFwInfo_t fwInfo {};
		SHCI_GetWirelessFwInfo(&fwInfo);

		sprintf(buf, "\r\nMountjoy Ball v1.0 - Current Settings:\r\n\r\n"
				"Connected: %s; handle: %#04x\r\n"
				"Last battery reading %d%%\r\n"
				"Wireless firmware: %s\r\n"
				"Sensitivity: %f\r\n"
				"Battery warning at %d%%; Update interval %lds\r\n"
				"Current Position (0-4096): %.1f, %.1f, %.1f\r\n"
				"BLE firmware version: %d.%d.%d.%d; FUS version: %d.%d.%d\r\n"
				"RSSI Value: %d dBm\r\n",
				bleApp.connectionStatus == BleApp::ConnectionStatus::ClientConnected ? "Yes" : "No",
				bleApp.connectionHandle,
				hidApp.batteryLevel,
				bleApp.coprocessorFailure ? "Off" : "Running",
				hidApp.settings.scaleMult,
				hidApp.settings.batteryWarning, hidApp.settings.batteryInterval,
				hidApp.position3D.x, hidApp.position3D.y, hidApp.position3D.z,
				fwInfo.VersionMajor, fwInfo.VersionMinor, fwInfo.VersionSub, fwInfo.VersionBranch,
				fwInfo.FusVersionMajor, fwInfo.FusVersionMinor, fwInfo.FusVersionSub,
				rssi
			);
		usb->SendString(buf);


	} else if (cmd.compare("help") == 0) {

		usb->SendString("Mountjoy Ball Base\r\n"
				"\r\nSupported commands:\r\n"
				"info               -  Current settings\r\n"
				"scan               -  List BLE devices\r\n"
				"connect            -  Connect to HID BLE device\r\n"
				"disconnect         -  Disconnect to HID BLE device\r\n"
				"sensitivity:x      -  Amount to scale down raw gyro data\r\n"
				"lowbatt:x          -  Flash LED when remote battery percent below x\r\n"
				"battinterval:x     -  Interval (seconds) to wake remote for battery check\r\n"
				"readgyro:HH        -  Read Gyro register\r\n"
				"writegyro:RR,VV    -  Write value 0xVV to gyro register 0xRR\r\n"
				"recenter           -  Recenter all channels\r\n"
				"output             -  Periodically output gyro and battery data\r\n"
				"battery            -  Get Battery Level\r\n"
				"hidmap:x12         -  Print HID report map at 12 hex digit device address\r\n"
				"saveconfig         -  Save current offsets to flash\r\n"
				"\r\n"

#if (USB_DEBUG)
				"usbdebug    -  Start USB debugging\r\n"
				"\r\n"
#endif
		);

#if (USB_DEBUG)
	} else if (cmd.compare("usbdebug") == 0) {						// Output USB debug data
		USBDebug = true;
		usb->SendString("Press link button to dump output\r\n");
#endif

	} else if (cmd.compare("connect") == 0) {						// Connect to HID device
		bleApp.SwitchConnectState();
		usb->SendString("Connecting ...\r\n");

	} else if (cmd.compare("scan") == 0) {							// List ble devices
		bleApp.ScanInfo();

	} else if (cmd.compare(0, 9, "readgyro:") == 0) {				// Read gyroscope register
		if (hidApp.state != HidApp::HidState::ClientConnected) {
			printf("Not connected\r\n");
		} else {
			uint8_t regNo;
			auto res = std::from_chars(cmd.data() + cmd.find(":") + 1, cmd.data() + cmd.size(), regNo, 16);

			if (res.ec == std::errc()) {
				hidApp.gyroCommand = regNo;
				hidApp.gyroCmdType = HidApp::GyroCmdType::read;
				UTIL_SEQ_SetTask(1 << CFG_TASK_GyroCommand, CFG_SCH_PRIO_0);
				printf("Requesting gyro register: %#02x\r\n", regNo);
			} else {
				usb->SendString("Invalid register\r\n");
			}
		}

	} else if (cmd.compare(0, 10, "writegyro:") == 0) {				// write value to gyroscope register

		if (hidApp.state != HidApp::HidState::ClientConnected) {
			printf("Not connected\r\n");
		} else {
			uint8_t regNo, value;
			auto res = std::from_chars(cmd.data() + cmd.find(":") + 1, cmd.data() + cmd.size(), regNo, 16);

			if (res.ec == std::errc()) {			// no error
				auto res = std::from_chars(cmd.data() + cmd.find(",") + 1, cmd.data() + cmd.size(), value, 16);
				if (res.ec == std::errc()) {			// no error
					hidApp.gyroCommand = regNo | value << 8;
					hidApp.gyroCmdType = HidApp::GyroCmdType::write;
					printf("Gyro write: Register: %#02x Value: %#02x\r\n", regNo, value);
					UTIL_SEQ_SetTask(1 << CFG_TASK_GyroCommand, CFG_SCH_PRIO_0);
				} else {
					usb->SendString("Invalid value\r\n");
				}
			} else {
				usb->SendString("Invalid register\r\n");
			}
		}

	} else if (cmd.compare(0, 7, "hidmap:") == 0) {					// Print Hid repot map for given address

		if (cmd.length() != 19) {
			usb->SendString("Address format not recognised\r\n");
		} else {
			uint8_t addr[BleApp::bleAddrSize];
			int8_t pos = cmd.find(":") + 1;							// locate position of character preceding
			size_t val = -1;
			for (int8_t i = BleApp::bleAddrSize; i > 0; --i) {
				addr[i - 1] = std::stoi(std::string(cmd.substr(pos, 2)), &val, 16);
				pos += 2;
			}

			bleApp.GetHidReportMap(addr);
		}

	} else if (cmd.compare("disconnect") == 0) {					// Disconnect
		bleApp.DisconnectRequest();

	} else if (cmd.compare("saveconfig") == 0) {					// Save offsets to flash
		config.SaveConfig();

	} else if (cmd.compare(0, 12, "sensitivity:") == 0) {			// Set sensitivity
		const float val = ParseFloat(cmd, ':');

		if (val > 0.0f) {
			hidApp.settings.scaleMult = val;
			printf("Scaler set to: %f\r\n", val);
		} else {
			printf("Invalid value\r\n");
		}
		config.SaveConfig();

	} else if (cmd.compare(0, 8, "lowbatt:") == 0) {				// Remote battery percent at which warning LED flashes
		const int8_t val = ParseInt(cmd, ':', 1, 99);
		if (val >= 0) {
			hidApp.settings.batteryWarning = val;
		}
		config.SaveConfig();

	} else if (cmd.compare(0, 13, "battinterval:") == 0) {			// Interval in seconds to wake remote for battery update
		const int32_t val = ParseInt(cmd, ':');
		if (val >= 0) {
			hidApp.settings.batteryInterval = val;
		}
		config.SaveConfig();

	} else if (cmd.compare("recenter") == 0) {						// Recenter all channels to mid point
		hidApp.position3D.x = 2047.0f;
		hidApp.position3D.y = 2047.0f;
		hidApp.position3D.z = 2047.0f;
		printf("All channels recentered\r\n");

	} else if (cmd.compare("output") == 0) {					// Output raw gyro data
		hidApp.outputGyro = !hidApp.outputGyro;
		printf("Gyro Output %s\r\n", hidApp.outputGyro ? "on" : "off");

	} else if (cmd.compare("battery") == 0) {						// read battery level
		UTIL_SEQ_SetTask(1 << CFG_TASK_GetBatteryLevel, CFG_SCH_PRIO_0);

	} else {
		PrintString("Unrecognised command: %s Type 'help' for supported commands\r\n", comCmd);
	}

	cmdPending = false;
}


void CDCHandler::PrintString(const char* format, ...)
{
	va_list args;
	va_start (args, format);
	vsnprintf (buf, bufSize, format, args);
	va_end (args);

	usb->SendString(buf);
}


char* CDCHandler::HexToString(const uint8_t* v, uint32_t len, const bool spaces) {
	const uint8_t byteLen = spaces ? 3 : 2;
	uint32_t pos = 0;
	len = std::min(maxStrLen / byteLen, len);

	for (uint8_t i = 0; i < len; ++i) {
		pos += sprintf(&stringBuf[pos], spaces ? "%02X " : "%02X", v[i]);
	}

	return (char*)&stringBuf;
}


char* CDCHandler::HexToString(const uint16_t v) {
	sprintf(stringBuf, "%04X", v);
	return (char*)&stringBuf;
}


void CDCHandler::DataIn()
{

}


// As this is called from an interrupt assign the command to a variable so it can be handled in the main loop
void CDCHandler::DataOut()
{
	// Check if sufficient space in command buffer
	const uint32_t newCharCnt = std::min(outBuffCount, maxCmdLen - 1 - buffPos);

	strncpy(&comCmd[buffPos], (char*)outBuff, newCharCnt);
	buffPos += newCharCnt;

	// Check if cr has been sent yet
	if (comCmd[buffPos - 1] == 13 || comCmd[buffPos - 1] == 10 || buffPos == maxCmdLen - 1) {
		comCmd[buffPos - 1] = '\0';
		cmdPending = true;
		buffPos = 0;
	}
}


void CDCHandler::ActivateEP()
{
	EndPointActivate(USBMain::CDC_In,   Direction::in,  EndPointType::Bulk);			// Activate CDC in endpoint
	EndPointActivate(USBMain::CDC_Out,  Direction::out, EndPointType::Bulk);			// Activate CDC out endpoint
	EndPointActivate(USBMain::CDC_Cmd,  Direction::in,  EndPointType::Interrupt);		// Activate Command IN EP

	EndPointTransfer(Direction::out, USBMain::CDC_Out, USBMain::ep_maxPacket);
}


void CDCHandler::ClassSetup(usbRequest& req)
{
	if (req.RequestType == DtoH_Class_Interface && req.Request == GetLineCoding) {
		SetupIn(req.Length, (uint8_t*)&lineCoding);
	}

	if (req.RequestType == HtoD_Class_Interface && req.Request == SetLineCoding) {
		// Prepare to receive line coding data in ClassSetupData
		usb->classPendingData = true;
		EndPointTransfer(Direction::out, 0, req.Length);
	}
}


void CDCHandler::ClassSetupData(usbRequest& req, const uint8_t* data)
{
	// ClassSetup passes instruction to set line coding - this is the data portion where the line coding is transferred
	if (req.RequestType == HtoD_Class_Interface && req.Request == SetLineCoding) {
		lineCoding = *(LineCoding*)data;
	}
}


int32_t CDCHandler::ParseInt(const std::string_view cmd, const char precedingChar, const int32_t low, const int32_t high) {
	int32_t val = -1;
	const int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(&cmd[pos + 1], "0123456789-") > 0) {
		val = std::stoi(&cmd[pos + 1]);
	}
	if (high > low && (val > high || val < low)) {
		printf("Must be a value between %ld and %ld\r\n", low, high);
		return low - 1;
	}
	return val;
}


float CDCHandler::ParseFloat(const std::string_view cmd, const char precedingChar, const float low, const float high) {
	float val = -1.0f;
	const int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(&cmd[pos + 1], "0123456789.") > 0) {
		val = std::stof(&cmd[pos + 1]);
	}
	if (high > low && (val > high || val < low)) {
		printf("Must be a value between %f and %f\r\n", low, high);
		return low - 1.0f;
	}
	return val;
}


// Descriptor definition here as requires constants from USB class
const uint8_t CDCHandler::Descriptor[] = {
	// IAD Descriptor - Interface association descriptor for CDC class
	0x08,									// bLength (8 bytes)
	USBMain::IadDescriptor,					// bDescriptorType
	USBMain::CDCCmdInterface,				// bFirstInterface
	0x02,									// bInterfaceCount
	0x02,									// bFunctionClass (Communications and CDC Control)
	0x02,									// bFunctionSubClass
	0x01,									// bFunctionProtocol
	USBMain::CommunicationClass,			// String Descriptor

	// Interface Descriptor
	0x09,									// bLength: Interface Descriptor size
	USBMain::InterfaceDescriptor,			// bDescriptorType: Interface
	USBMain::CDCCmdInterface,				// bInterfaceNumber: Number of Interface
	0x00,									// bAlternateSetting: Alternate setting
	0x01,									// bNumEndpoints: 1 endpoint used
	0x02,									// bInterfaceClass: Communication Interface Class
	0x02,									// bInterfaceSubClass: Abstract Control Model
	0x01,									// bInterfaceProtocol: Common AT commands
	USBMain::CommunicationClass,			// iInterface

	// Header Functional Descriptor
	0x05,									// bLength: Endpoint Descriptor size
	USBMain::ClassSpecificInterfaceDescriptor,	// bDescriptorType: CS_INTERFACE
	0x00,									// bDescriptorSubtype: Header Func Desc
	0x10,									// bcdCDC: spec release number
	0x01,

	// Call Management Functional Descriptor
	0x05,									// bFunctionLength
	USBMain::ClassSpecificInterfaceDescriptor,	// bDescriptorType: CS_INTERFACE
	0x01,									// bDescriptorSubtype: Call Management Func Desc
	0x00,									// bmCapabilities: D0+D1
	0x01,									// bDataInterface: 1

	// ACM Functional Descriptor
	0x04,									// bFunctionLength
	USBMain::ClassSpecificInterfaceDescriptor,	// bDescriptorType: CS_INTERFACE
	0x02,									// bDescriptorSubtype: Abstract Control Management desc
	0x02,									// bmCapabilities

	// Union Functional Descriptor
	0x05,									// bFunctionLength
	USBMain::ClassSpecificInterfaceDescriptor,	// bDescriptorType: CS_INTERFACE
	0x06,									// bDescriptorSubtype: Union func desc
	0x00,									// bMasterInterface: Communication class interface
	0x01,									// bSlaveInterface0: Data Class Interface

	// Endpoint 2 Descriptor
	0x07,									// bLength: Endpoint Descriptor size
	USBMain::EndpointDescriptor,			// bDescriptorType: Endpoint
	USBMain::CDC_Cmd,						// bEndpointAddress
	USBMain::Interrupt,						// bmAttributes: Interrupt
	0x08,									// wMaxPacketSize
	0x00,
	0x10,									// bInterval

	//---------------------------------------------------------------------------

	// Data class interface descriptor
	0x09,									// bLength: Endpoint Descriptor size
	USBMain::InterfaceDescriptor,			// bDescriptorType:
	USBMain::CDCDataInterface,				// bInterfaceNumber: Number of Interface
	0x00,									// bAlternateSetting: Alternate setting
	0x02,									// bNumEndpoints: Two endpoints used
	0x0A,									// bInterfaceClass: CDC
	0x00,									// bInterfaceSubClass:
	0x00,									// bInterfaceProtocol:
	0x00,									// iInterface:

	// Endpoint OUT Descriptor
	0x07,									// bLength: Endpoint Descriptor size
	USBMain::EndpointDescriptor,			// bDescriptorType: Endpoint
	USBMain::CDC_Out,						// bEndpointAddress
	USBMain::Bulk,							// bmAttributes: Bulk
	LOBYTE(USBMain::ep_maxPacket),			// wMaxPacketSize:
	HIBYTE(USBMain::ep_maxPacket),
	0x00,									// bInterval: ignore for Bulk transfer

	// Endpoint IN Descriptor
	0x07,									// bLength: Endpoint Descriptor size
	USBMain::EndpointDescriptor,			// bDescriptorType: Endpoint
	USBMain::CDC_In,						// bEndpointAddress
	USBMain::Bulk,							// bmAttributes: Bulk
	LOBYTE(USBMain::ep_maxPacket),			// wMaxPacketSize:
	HIBYTE(USBMain::ep_maxPacket),
	0x00,									// bInterval: ignore for Bulk transfer
};


uint32_t CDCHandler::GetInterfaceDescriptor(const uint8_t** buffer) {
	*buffer = Descriptor;
	return sizeof(Descriptor);
}

