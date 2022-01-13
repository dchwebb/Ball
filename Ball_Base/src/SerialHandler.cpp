#include "SerialHandler.h"
#include "app_ble.h"
#include "ble_hal_aci.h"
#include <stdio.h>
//#include <cmath>		// for cordic test

int32_t SerialHandler::ParseInt(const std::string cmd, const char precedingChar, int low = 0, int high = 0) {
	int32_t val = -1;
	int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(cmd.substr(pos + 1).c_str(), "0123456789-") > 0) {
		val = stoi(cmd.substr(pos + 1));
	}
	if (high > low && (val > high || val < low)) {
		usb->SendString("Must be a value between " + std::to_string(low) + " and " + std::to_string(high) + "\r\n");
		return low - 1;
	}
	return val;
}

float SerialHandler::ParseFloat(const std::string cmd, const char precedingChar, float low = 0.0, float high = 0.0) {
	float val = -1.0f;
	int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(cmd.substr(pos + 1).c_str(), "0123456789.") > 0) {
		val = stof(cmd.substr(pos + 1));
	}
	if (high > low && (val > high || val < low)) {
		usb->SendString("Must be a value between " + std::to_string(low) + " and " + std::to_string(high) + "\r\n");
		return low - 1.0f;
	}
	return val;
}

SerialHandler::SerialHandler(USBHandler& usbObj)
{
	usb = &usbObj;

	// bind the usb's CDC caller to the CDC handler in this class
	usb->cdcDataHandler = std::bind(&SerialHandler::Handler, this, std::placeholders::_1, std::placeholders::_2);
}



// Check if a command has been received from USB, parse and action as required
bool SerialHandler::Command()
{
	//char buf[50];

	if (!CmdPending) {
		return false;
	}

	// Provide option to switch to USB DFU mode - this allows the MCU to be programmed with STM32CubeProgrammer in DFU mode
	if (state == serialState::dfuConfirm) {
		if (ComCmd.compare("y\n") == 0 || ComCmd.compare("Y\n") == 0) {
			usb->SendString("Switching to DFU Mode ...\r\n");
			uint32_t old = uwTick;
			while (uwTick < old + 100) {};		// Give enough time to send the message
			//bootloader.BootDFU();
		} else {
			state = serialState::pending;
			usb->SendString("Upgrade cancelled\r\n");
		}

	} else if (ComCmd.compare("info\n") == 0) {		// Print diagnostic information

		usb->SendString("Mountjoy Ball v1.0 - Current Settings:\r\n\r\n");

	} else if (ComCmd.compare("help\n") == 0) {

		usb->SendString("Mountjoy Ball Base\r\n"
				"\r\nSupported commands:\r\n"
				"scan               -  List BLE devices\r\n"
				"connect            -  Connect to HID BLE device\r\n"
				"disconnect         -  Disconnect to HID BLE device\r\n"
				"hidmap:xxxxxxxxxx  -  Print HID report map given device address\r\n"
				"\r\n"
#if (USB_DEBUG)
				"usbdebug    -  Start USB debugging\r\n"
				"\r\n"
#endif
		);

#if (USB_DEBUG)
	} else if (ComCmd.compare("usbdebug\n") == 0) {				// Configure gate LED
		USBDebug = true;
		usb->SendString("Press link button to dump output\r\n");
#endif

	} else if (ComCmd.compare("connect\n") == 0) {				// Connect to HID device
		bleApp.ScanAndConnect();
		usb->SendString("Connecting ...\r\n");

	} else if (ComCmd.compare("scan\n") == 0) {					// List ble devices
		bleApp.ScanInfo();

	} else if (ComCmd.compare(0, 7, "hidmap:") == 0) {			// Print Hid repot map for given address

		if (ComCmd.length() != 20) {
			usb->SendString("Address format not recognised\r\n");
		} else {
			uint8_t addr[BleApp::bdddrSize];
			int8_t pos = ComCmd.find(":") + 1;								// locate position of character preceding
			size_t val = -1;
			for (int8_t i = BleApp::bdddrSize; i > 0; --i) {
				addr[i - 1] = stoi(ComCmd.substr(pos, 2), &val, 16);
				pos += 2;
			}

			bleApp.GetHidReportMap(addr);
		}

	} else if (ComCmd.compare("disconnect\n") == 0) {			// Disconnect
		bleApp.DisconnectRequest();

	} else if (ComCmd.compare("fwversion\n") == 0) {			// Version of BLE firmware
//		uint16_t version;
//		if (aci_hal_get_fw_build_number(&version) == 0) {
//			printf("BLE firmware version: %d\r\n", version);
//		}

	} else {
		usb->SendString("Unrecognised command: " + ComCmd + "Type 'help' for supported commands\r\n");
	}

	CmdPending = false;
	return true;
}


void SerialHandler::Handler(uint8_t* data, uint32_t length)
{
	static bool newCmd = true;
	if (newCmd) {
		ComCmd = std::string(reinterpret_cast<char*>(data), length);
		newCmd = false;
	} else {
		ComCmd.append(reinterpret_cast<char*>(data), length);
	}
	if (*ComCmd.rbegin() == '\r')
		*ComCmd.rbegin() = '\n';

	if (*ComCmd.rbegin() == '\n') {
		CmdPending = true;
		newCmd = true;
	}

}

