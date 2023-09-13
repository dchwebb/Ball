#include "USB.h"
#include "CDCHandler.h"
#include <charconv>
extern "C" {
#include "shci.h"
#include "ble_hal_aci.h"
#include "ble_hci_le.h"
}
#include "stm32_seq.h"
#include "app_ble.h"
#include "ble_hid.h"


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
			uint32_t old = uwTick;
			while (uwTick < old + 100) {};		// Give enough time to send the message
			//bootloader.BootDFU();
		} else {
			state = serialState::pending;
			usb->SendString("Upgrade cancelled\r\n");
		}

	} else if (cmd.compare("info") == 0) {		// Print diagnostic information

		sprintf(buf, "\r\nMountjoy Ball v1.0 - Current Settings:\r\n\r\n"
				"Wireless firmware: %s\r\n"
				"Offsets: %f, %f, %f\r\n"
				"Sensitivity: %f\r\n"
				"Current Position (0-4096): %d, %d, %d\r\n",
				bleApp.coprocessorFailure ? "Off" : "Running",
				hidApp.offsetX, hidApp.offsetY, hidApp.offsetZ,
				hidApp.divider,
				hidApp.position3D.x, hidApp.position3D.y, hidApp.position3D.z);
		usb->SendString(buf);


	} else if (cmd.compare("help") == 0) {

		usb->SendString("Mountjoy Ball Base\r\n"
				"\r\nSupported commands:\r\n"
				"info               -  Current settings\r\n"
				"scan               -  List BLE devices\r\n"
				"connect            -  Connect to HID BLE device\r\n"
				"disconnect         -  Disconnect to HID BLE device\r\n"
				"hidmap:xxxxxxxxxx  -  Print HID report map given device address\r\n"
				"sensitivity:x      -  Amount to divide raw gyro data\r\n"
				"offset:x=?         -  X/Y/Z offset (more negative if falling)\r\n"
				"calibrate          -  Recenter and calibrate gyro offsets\r\n"
				"recenter           -  Recenter all channels\r\n"
				"outputgyro         -  Periodically output raw gyro data\r\n"
				"fwversion          -  Read firmware version\r\n"
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
		bleApp.ScanAndConnect();
		usb->SendString("Connecting ...\r\n");

	} else if (cmd.compare("scan") == 0) {							// List ble devices
		bleApp.ScanInfo();

	} else if (cmd.compare(0, 7, "hidmap:") == 0) {					// Print Hid repot map for given address

		if (cmd.length() != 20) {
			usb->SendString("Address format not recognised\r\n");
		} else {
			uint8_t addr[BleApp::bdddrSize];
			int8_t pos = cmd.find(":") + 1;							// locate position of character preceding
			size_t val = -1;
			for (int8_t i = BleApp::bdddrSize; i > 0; --i) {
				addr[i - 1] = std::stoi(cmd.substr(pos, 2).data(), &val, 16);
				pos += 2;
			}

			bleApp.GetHidReportMap(addr);
		}

	} else if (cmd.compare("disconnect") == 0) {					// Disconnect
		bleApp.DisconnectRequest();

	} else if (cmd.compare("fwversion") == 0) {						// Version of BLE firmware
		WirelessFwInfo_t fwInfo;
		if (SHCI_GetWirelessFwInfo(&fwInfo) == 0) {
			printf("BLE firmware version: %d.%d.%d.%d; FUS version: %d.%d.%d\r\n",
					fwInfo.VersionMajor, fwInfo.VersionMinor, fwInfo.VersionSub, fwInfo.VersionBranch,
					fwInfo.FusVersionMajor, fwInfo.FusVersionMinor, fwInfo.FusVersionSub);
		}

	} else if (cmd.compare(0, 12, "sensitivity:") == 0) {			// Set sensitivity
		uint16_t div;
		auto res = std::from_chars(cmd.data() + cmd.find(":") + 1, cmd.data() + cmd.size(), div, 10);

		if (res.ec == std::errc()) {
			hidApp.divider = div;
			printf("Divider set to: %d\r\n", div);
		} else {
			printf("Invalid value\r\n");
		}

	} else if (cmd.compare("recenter") == 0) {						// Recenter all channels to mid point
		hidApp.position3D.x = 2047;
		hidApp.position3D.y = 2047;
		hidApp.position3D.z = 2047;
		printf("All channels recentered\r\n");

	} else if (cmd.compare(0, 7, "offset:") == 0) {					// Set x offset for raw calibration data
		int16_t offset;
		auto res = std::from_chars(cmd.data() + cmd.find("=") + 1, cmd.data() + cmd.size(), offset, 10);

		if (res.ec == std::errc()) {
			if (cmd.compare(7, 1, "x") == 0) {
				hidApp.offsetX = offset;
			} else if (cmd.compare(7, 1, "y") == 0) {
				hidApp.offsetY = offset;
			} else if (cmd.compare(7, 1, "z") == 0) {
				hidApp.offsetZ = offset;
			}
			printf("Offset set to: %d\r\n", offset);
		} else {
			printf("Invalid value\r\n");
		}

	} else if (cmd.compare("calibrate") == 0) {						// recenter and calibrate gyro
		if (hidApp.state != HidApp::HidState::ClientConnected) {
			printf("Must be connected before calibrating\r\n");
		} else {
			printf("Starting calibration\r\n");
			hidApp.Calibrate();
		}

	} else if (cmd.compare("outputgyro") == 0) {					// Output raw gyro data
		hidApp.outputGyro = !hidApp.outputGyro;

	} else {
		usb->SendString("Unrecognised command: " + std::string(comCmd) + " Type 'help' for supported commands\r\n");
	}

	cmdPending = false;
}


void CDCHandler::DataIn()
{
	if (inBuffSize > 0 && inBuffSize % USBMain::ep_maxPacket == 0) {
		inBuffSize = 0;
		EndPointTransfer(Direction::in, inEP, 0);					// Fixes issue transmitting an exact multiple of max packet size (n x 64)
	}
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

