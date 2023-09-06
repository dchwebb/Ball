#include "SerialHandler.h"

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

std::string HexByte(const uint16_t& v) {
	char buf[50];
	sprintf(buf, "%X", v);
	return std::string(buf);

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

		sprintf(printBuffer, "\r\nMountjoy Ball Remote v1.0 - Current Settings:\r\n\r\n"
				"Battery: %f V\r\n"
				"Wireless Stack: %s\r\n",
				basService.GetBatteryLevel(),
				(coprocessorFailure ? "Off" : "Running"));

		usb->SendString(printBuffer);


	} else if (comCmd.compare("help\n") == 0) {

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
	} else if (comCmd.compare("usbdebug\n") == 0) {				// Configure gate LED
		USBDebug = true;
		usb->SendString("Press link button to dump output\r\n");
#endif

	} else if (comCmd.compare("outputgyro\n") == 0) {					// Output raw gyro data
		hidService.outputGyro = !hidService.outputGyro;
		if (!hidService.JoystickNotifications) {					// If not outputting to BLE client start timer interrupt
			gyro.ContinualRead(hidService.outputGyro);
		}


	} else if (comCmd.compare("gyroread\n") == 0) {					// Trigger a repeated read
		if (hidService.JoystickNotifications) {
			printf("Currently connected\r\n");
		} else {
			gyro.GyroRead();
			printf("x: %d, y:%d, z: %d\n", gyro.gyroData.x, gyro.gyroData.y, gyro.gyroData.z);
		}

	} else if (comCmd.compare(0, 8, "readspi:") == 0) {				// Read spi register
		if (hidService.JoystickNotifications) {
			usb->SendString("Currently connected\r\n");
		} else {

			uint8_t regNo;
			auto res = std::from_chars(comCmd.data() + comCmd.find(":") + 1, comCmd.data() + comCmd.size(), regNo, 16);

			if (res.ec == std::errc()) {
				uint8_t readData = gyro.ReadRegister(regNo);
				usb->SendString("I2C Register: 0x" + HexByte(regNo) + " Value: 0x" + HexByte(readData) + "\r\n");
			} else {
				usb->SendString("Invalid register\r\n");
			}
		}

	} else if (comCmd.compare(0, 9, "writespi:") == 0) {			// write i2c register
		if (hidService.JoystickNotifications) {
			usb->SendString("Currently connected\r\n");
		} else {

			uint8_t regNo, value;
			auto res = std::from_chars(comCmd.data() + comCmd.find(":") + 1, comCmd.data() + comCmd.size(), regNo, 16);

			if (res.ec == std::errc()) {			// no error
				auto res = std::from_chars(comCmd.data() + comCmd.find(",") + 1, comCmd.data() + comCmd.size(), value, 16);
				if (res.ec == std::errc()) {			// no error
					gyro.WriteCmd(regNo, value);
					usb->SendString("I2C write: Register: 0x" + HexByte(regNo) + " Value: 0x" + HexByte(value) + "\r\n");
				} else {
					usb->SendString("Invalid value\r\n");
				}
			} else {
				usb->SendString("Invalid register\r\n");
			}
		}


	} else if (comCmd.compare("rssi\n") == 0) {					// RSSI value
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

	} else if (comCmd.compare("fwversion\n") == 0) {			// Version of BLE firmware
		WirelessFwInfo_t fwInfo;
		if (SHCI_GetWirelessFwInfo(&fwInfo) == 0) {
			printf("BLE firmware version: %d.%d.%d.%d; FUS version: %d.%d.%d\r\n",
					fwInfo.VersionMajor, fwInfo.VersionMinor, fwInfo.VersionSub, fwInfo.VersionBranch,
					fwInfo.FusVersionMajor, fwInfo.FusVersionMinor, fwInfo.FusVersionSub);
		}

	} else if (comCmd.compare("sleep\n") == 0) {				// Enter sleep mode
		usb->SendString("Going to sleep\n");
		extern bool sleep;
		sleep = true;		// Triggers idle routine UTIL_SEQ_Idle() in app_entry.c

	} else if (comCmd.compare("shutdown\n") == 0) {				// Enter sleep mode
		usb->SendString("Shutting down\n");
		bleApp.lowPowerMode = BleApp::LowPowerMode::Shutdown;
		extern bool sleep;
		sleep = true;		// Triggers idle routine UTIL_SEQ_Idle() in app_entry.c

	} else if (comCmd.compare("canceladv\n") == 0) {			// Cancel advertising
		UTIL_SEQ_SetTask(1 << CFG_TASK_CancelAdvertising, CFG_SCH_PRIO_0);

	} else if (comCmd.compare("startadv\n") == 0) {				// Start advertising
		UTIL_SEQ_SetTask(1 << CFG_TASK_SwitchFastAdvertising, CFG_SCH_PRIO_0);

	} else if (comCmd.compare("disconnect\n") == 0) {				// Disconnect client
		bleApp.DisconnectRequest();

	} else {
		usb->SendString("Unrecognised command: " + std::string(comCmd) + "Type 'help' for supported commands\r\n");
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

