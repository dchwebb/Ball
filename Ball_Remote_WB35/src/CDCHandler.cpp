#include "USB.h"
#include "CDCHandler.h"
#include <charconv>
extern "C" {
#include "shci.h"
#include "ble_hal_aci.h"
#include "ble_hci_le.h"
}
#include "gyroSPI.h"
#include "stm32_seq.h"
#include "app_ble.h"
#include "hids_app.h"
#include "bas_app.h"



// Check if a command has been received from USB, parse and action as required
void CDCHandler::ProcessCommand()
{
	if (!cmdPending) {
		return;
	}

	const std::string_view cmd {comCmd};

	// Provide option to switch to USB DFU mode - this allows the MCU to be programmed with STM32CubeProgrammer in DFU mode
	if (state == serialState::dfuConfirm) {
		if (cmd.compare("y\n") == 0 || cmd.compare("Y\n") == 0) {
			usb->SendString("Switching to DFU Mode ...\r\n");
			uint32_t old = SysTickVal;
			while (SysTickVal < old + 100) {};		// Give enough time to send the message
			//bootloader.BootDFU();
		} else {
			state = serialState::pending;
			usb->SendString("Upgrade cancelled\r\n");
		}

	} else if (cmd.compare("info") == 0) {		// Print diagnostic information

		int8_t rssi = -127;
		if (bleApp.connectionStatus == BleApp::ConnStatus::Connected) {
			hci_read_rssi(bleApp.connectionHandle, (uint8_t*)&rssi);
		}

		WirelessFwInfo_t fwInfo {};
		SHCI_GetWirelessFwInfo(&fwInfo);

		sprintf(buf, "\r\nMountjoy Ball Remote v1.0 - Current Settings:\r\n\r\n"
				"Battery: %.2fv  %d%%\r\n"
				"Wireless Stack: %s\r\n"
				"BLE firmware version: %d.%d.%d.%d; FUS version: %d.%d.%d\r\n"
				"RSSI Value: %d dBm\r\n",
				basService.GetBatteryLevel(), basService.Level,
				(bleApp.coprocessorFailure ? "Off" : "Running"),
				fwInfo.VersionMajor, fwInfo.VersionMinor, fwInfo.VersionSub, fwInfo.VersionBranch,
				fwInfo.FusVersionMajor, fwInfo.FusVersionMinor, fwInfo.FusVersionSub,
				rssi);

		usb->SendString(buf);


	} else if (cmd.compare("help") == 0) {

		usb->SendString("Mountjoy Ball Remote\r\n"
				"\r\nSupported commands:\r\n"
				"readspi:HH      -  Read gyro register at 0xHH\r\n"
				"writespi:RR,VV  -  Write value 0xVV to gyro register 0xRR\r\n"
				"fwversion       -  Read firmware version\r\n"
				"sleep           -  Enter sleep mode\r\n"
				"shutdown        -  Enter shutdown mode\r\n"
				"canceladv       -  Cancel advertising\r\n"
				"startadv        -  Start advertising\r\n"
				"disconnect      -  Disconnects clients\r\n"
				"gyroread        -  Returns gyro x, y and z\r\n"
				"outputgyro      -  Periodically output raw gyro data\r\n"
				"rssi            -  Get RSSI value\r\n"
				"\r\n"

#if (USB_DEBUG)
				"usbdebug    -  Start USB debugging\r\n"
				"\r\n"
#endif
		);

#if (USB_DEBUG)
	} else if (cmd.compare("usbdebug") == 0) {				// Configure gate LED
		USBDebug = true;
		usb->SendString("Press link button to dump output\r\n");
#endif

	} else if (cmd.compare("outputgyro") == 0) {					// Output raw gyro data
		hidService.outputGyro = !hidService.outputGyro;
		if (!hidService.JoystickNotifications) {					// If not outputting to BLE client start timer interrupt
			gyro.ContinualRead(hidService.outputGyro);
		}


	} else if (cmd.compare("gyroread") == 0) {						// Trigger a gyroscope read
		if (!hidService.JoystickNotifications) {
			gyro.GyroRead();
		}
		printf("x: %d, y:%d, z: %d\n", gyro.gyroData.x, gyro.gyroData.y, gyro.gyroData.z);

	} else if (cmd.compare(0, 8, "readspi:") == 0) {				// Read spi register
		if (hidService.JoystickNotifications) {
			usb->SendString("Currently connected\r\n");
		} else {

			uint8_t regNo;
			auto res = std::from_chars(cmd.data() + cmd.find(":") + 1, cmd.data() + cmd.size(), regNo, 16);

			if (res.ec == std::errc()) {
				uint8_t readData = gyro.ReadRegister(regNo);
				printf("I2C Register: %#04x Value: %#04x\r\n", regNo, readData);
			} else {
				usb->SendString("Invalid register\r\n");
			}
		}

	} else if (cmd.compare(0, 9, "writespi:") == 0) {			// write i2c register
		if (hidService.JoystickNotifications) {
			usb->SendString("Currently connected\r\n");
		} else {

			uint8_t regNo, value;
			auto res = std::from_chars(cmd.data() + cmd.find(":") + 1, cmd.data() + cmd.size(), regNo, 16);

			if (res.ec == std::errc()) {			// no error
				auto res = std::from_chars(cmd.data() + cmd.find(",") + 1, cmd.data() + cmd.size(), value, 16);
				if (res.ec == std::errc()) {			// no error
					gyro.WriteCmd(regNo, value);
					printf("SPI write: Register: %#04x Value: %#04x\r\n", regNo, value);
				} else {
					usb->SendString("Invalid value\r\n");
				}
			} else {
				usb->SendString("Invalid register\r\n");
			}
		}


	} else if (cmd.compare("rssi") == 0) {					// RSSI value
		if (bleApp.connectionStatus != BleApp::ConnStatus::Connected) {
			printf("Not connected\r\n");
		} else {
			int8_t rssi = 0;
			if (hci_read_rssi(bleApp.connectionHandle, (uint8_t*)&rssi) == 0) {
				printf("RSSI Value: %d dBm\r\n", rssi);
			} else {
				printf("Error reading RSSI Value\r\n");
			}
		}

	} else if (cmd.compare("fwversion") == 0) {			// Version of BLE firmware
		WirelessFwInfo_t fwInfo;
		if (SHCI_GetWirelessFwInfo(&fwInfo) == 0) {
			printf("BLE firmware version: %d.%d.%d.%d; FUS version: %d.%d.%d\r\n",
					fwInfo.VersionMajor, fwInfo.VersionMinor, fwInfo.VersionSub, fwInfo.VersionBranch,
					fwInfo.FusVersionMajor, fwInfo.FusVersionMinor, fwInfo.FusVersionSub);
		}

	} else if (cmd.compare("sleep") == 0) {				// Enter sleep mode
		usb->SendString("Going to sleep\n");
		extern bool sleep;
		sleep = true;		// Triggers idle routine UTIL_SEQ_Idle() in app_entry.c

	} else if (cmd.compare("shutdown") == 0) {				// Enter sleep mode
		usb->SendString("Shutting down\n");
		bleApp.lowPowerMode = BleApp::LowPowerMode::Shutdown;
		extern bool sleep;
		sleep = true;		// Triggers idle routine UTIL_SEQ_Idle() in app_entry.c

	} else if (cmd.compare("canceladv") == 0) {			// Cancel advertising
		UTIL_SEQ_SetTask(1 << CFG_TASK_CancelAdvertising, CFG_SCH_PRIO_0);

	} else if (cmd.compare("startadv") == 0) {				// Start advertising
		UTIL_SEQ_SetTask(1 << CFG_TASK_SwitchFastAdvertising, CFG_SCH_PRIO_0);

	} else if (cmd.compare("disconnect") == 0) {				// Disconnect client
		bleApp.DisconnectRequest();

	} else {
		usb->SendString("Unrecognised command: " + std::string(cmd) + "Type 'help' for supported commands\r\n");
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


int32_t CDCHandler::ParseInt(const std::string_view cmd, const char precedingChar, const int32_t low = 0, const int32_t high = 0) {
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


float CDCHandler::ParseFloat(const std::string_view cmd, const char precedingChar, const float low = 0.0f, const float high = 0.0f) {
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

