#include "SerialHandler.h"
#include "app_ble.h"
#include "ble_hid.h"
#include "ble_hal_aci.h"
#include <stdio.h>
extern "C" {
#include "shci.h"
}
#include <charconv>

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
	if (!CmdPending) {
		return false;
	}

	// Provide option to switch to USB DFU mode - this allows the MCU to be programmed with STM32CubeProgrammer in DFU mode
	if (state == serialState::dfuConfirm) {
		if (comCmd.compare("y\n") == 0 || comCmd.compare("Y\n") == 0) {
			usb->SendString("Switching to DFU Mode ...\r\n");
			uint32_t old = uwTick;
			while (uwTick < old + 100) {};		// Give enough time to send the message
			//bootloader.BootDFU();
		} else {
			state = serialState::pending;
			usb->SendString("Upgrade cancelled\r\n");
		}

	} else if (comCmd.compare("info\n") == 0) {		// Print diagnostic information
		extern bool coprocessorFailure;

		sprintf(printBuffer, "\r\nMountjoy Ball v1.0 - Current Settings:\r\n\r\n"
				"Wireless firmware: %s\r\n"
				"Offsets: %f, %f, %f\r\n"
				"Sensitivity: %f\r\n"
				"Current Position (0-4096): %d, %d, %d\r\n",
				coprocessorFailure ? "Off" : "Running",
				hidApp.offsetX, hidApp.offsetY, hidApp.offsetZ,
				hidApp.divider,
				hidApp.position3D.x, hidApp.position3D.y, hidApp.position3D.z);
		usb->SendString(printBuffer);

//		printf("\r\nMountjoy Ball v1.0 - Current Settings:\r\n\r\n");
//		printf("Offsets: %f, %f, %f\r\n", hidApp.offsetX, hidApp.offsetY, hidApp.offsetZ);
//		printf("Sensitivity: %f\r\n", hidApp.divider);
//		printf("Current Position (0-4096): %d, %d, %d\r\n", hidApp.position3D.x, hidApp.position3D.y, hidApp.position3D.z);


	} else if (comCmd.compare("help\n") == 0) {

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
	} else if (comCmd.compare("usbdebug\n") == 0) {				// Configure gate LED
		USBDebug = true;
		usb->SendString("Press link button to dump output\r\n");
#endif

	} else if (comCmd.compare("connect\n") == 0) {				// Connect to HID device
		bleApp.ScanAndConnect();
		usb->SendString("Connecting ...\r\n");

	} else if (comCmd.compare("scan\n") == 0) {					// List ble devices
		bleApp.ScanInfo();

	} else if (comCmd.compare(0, 7, "hidmap:") == 0) {			// Print Hid repot map for given address

		if (comCmd.length() != 20) {
			usb->SendString("Address format not recognised\r\n");
		} else {
			uint8_t addr[BleApp::bdddrSize];
			int8_t pos = comCmd.find(":") + 1;					// locate position of character preceding
			size_t val = -1;
			for (int8_t i = BleApp::bdddrSize; i > 0; --i) {
				addr[i - 1] = stoi(comCmd.substr(pos, 2), &val, 16);
				pos += 2;
			}

			bleApp.GetHidReportMap(addr);
		}

	} else if (comCmd.compare("disconnect\n") == 0) {			// Disconnect
		bleApp.DisconnectRequest();

	} else if (comCmd.compare("fwversion\n") == 0) {			// Version of BLE firmware
		WirelessFwInfo_t fwInfo;
		if (SHCI_GetWirelessFwInfo(&fwInfo) == 0) {
			printf("BLE firmware version: %d.%d.%d.%d; FUS version: %d.%d.%d\r\n",
					fwInfo.VersionMajor, fwInfo.VersionMinor, fwInfo.VersionSub, fwInfo.VersionBranch,
					fwInfo.FusVersionMajor, fwInfo.FusVersionMinor, fwInfo.FusVersionSub);
		}

	} else if (comCmd.compare(0, 12, "sensitivity:") == 0) {	// Set sensitivity
		uint16_t div;
		auto res = std::from_chars(comCmd.data() + comCmd.find(":") + 1, comCmd.data() + comCmd.size(), div, 10);

		if (res.ec == std::errc()) {
			hidApp.divider = div;
			printf("Divider set to: %d\r\n", div);
		} else {
			printf("Invalid value\r\n");
		}

	} else if (comCmd.compare("recenter\n") == 0) {				// Recenter all channels to mid point
		hidApp.position3D.x = 2047;
		hidApp.position3D.y = 2047;
		hidApp.position3D.z = 2047;
		printf("All channels recentered\r\n");

	} else if (comCmd.compare(0, 7, "offset:") == 0) {			// Set x offset for raw calibration data
		int16_t offset;
		auto res = std::from_chars(comCmd.data() + comCmd.find("=") + 1, comCmd.data() + comCmd.size(), offset, 10);

		if (res.ec == std::errc()) {
			if (comCmd.compare(7, 1, "x") == 0) {
				hidApp.offsetX = offset;
			} else if (comCmd.compare(7, 1, "y") == 0) {
				hidApp.offsetY = offset;
			} else if (comCmd.compare(7, 1, "z") == 0) {
				hidApp.offsetZ = offset;
			}
			printf("Offset set to: %d\r\n", offset);
		} else {
			printf("Invalid value\r\n");
		}

	} else if (comCmd.compare("calibrate\n") == 0) {			// recenter and calibrate gyro
		if (hidApp.state != HidApp::HidState::ClientConnected) {
			printf("Must be connected before calibrating\r\n");
		} else {
			printf("Starting calibration\r\n");
			hidApp.Calibrate();
		}

	} else if (comCmd.compare("outputgyro\n") == 0) {					// Output raw gyro data
		hidApp.outputGyro = !hidApp.outputGyro;

	} else {
		usb->SendString("Unrecognised command: " + comCmd + "Type 'help' for supported commands\r\n");
	}

	CmdPending = false;
	return true;
}


void SerialHandler::Handler(uint8_t* data, uint32_t length)
{
	static bool newCmd = true;
	if (newCmd) {
		comCmd = std::string(reinterpret_cast<char*>(data), length);
		newCmd = false;
	} else {
		comCmd.append(reinterpret_cast<char*>(data), length);
	}
	if (*comCmd.rbegin() == '\r')
		*comCmd.rbegin() = '\n';

	if (*comCmd.rbegin() == '\n') {
		CmdPending = true;
		newCmd = true;
	}

}

