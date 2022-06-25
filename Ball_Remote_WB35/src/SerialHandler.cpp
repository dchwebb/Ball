#include "SerialHandler.h"
#include "gyroHandler.h"
#include "app_ble.h"
#include "hids_app.h"
#include "ble_hal_aci.h"
#include <stdio.h>
extern "C" {
#include "shci.h"
}
#include "stm32_seq.h"
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

		// Get Battery level: 4096 / 2.8V = 1462; voltage divider calculated to scale by 1.73 (values corrected by measurement)
		ADC1->ISR &= ~ADC_ISR_EOC;
		ADC1->CR |= ADC_CR_ADSTART;
		while ((ADC1->ISR & ADC_ISR_EOC) != ADC_ISR_EOC) {}
		float voltage = (static_cast<float>(ADC1->DR) / 1390.0f) * 1.73f;

		sprintf(printBuffer, "\r\nMountjoy Ball Remote v1.0 - Current Settings:\r\n\r\n"
				"Battery: %f V\r\n"
				"Battery ADC: %u V\r\n"
				"Wireless Stack: %s\r\n",
				voltage,
				ADC1->DR,
				(coprocessorFailure ? "Off" : "Running"));

		usb->SendString(printBuffer);


	} else if (comCmd.compare("help\n") == 0) {

		usb->SendString("Mountjoy Ball Remote\r\n"
				"\r\nSupported commands:\r\n"
				"readi2c:HH      -  Read I2C register at 0xHH\r\n"
				"writei2c:RR,VV  -  Write value 0xVV to I2C register 0xRR\r\n"
				"i2cscan         -  Scan for valid I2C addresses\r\n"
				"fwversion       -  Read firmware version\r\n"
				"sleep           -  Enter sleep mode\r\n"
				"shutdown        -  Enter shutdown mode\r\n"
				"canceladv       -  Cancel advertising\r\n"
				"startadv        -  Start advertising\r\n"
				"disconnect      -  Disconnects clients\r\n"
				"gyroread        -  Returns gyro x, y and z\r\n"
				"outputgyro      -  Periodically output raw gyro data\r\n"
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


	} else if (comCmd.compare("gyroread\n") == 0) {					// Trigger a repeated read
		if (hidService.JoystickNotifications) {
			usb->SendString("Currently connected\r\n");
		} else {
			gyro.DebugRead();
		}

	} else if (comCmd.compare(0, 8, "readi2c:") == 0) {				// Read i2c register
		if (hidService.JoystickNotifications) {
			usb->SendString("Currently connected\r\n");
		} else {

			uint8_t regNo;
			auto res = std::from_chars(comCmd.data() + comCmd.find(":") + 1, comCmd.data() + comCmd.size(), regNo, 16);

			if (res.ec == std::errc()) {
				uint8_t readData = gyro.ReadData(regNo);
				usb->SendString("I2C Register: 0x" + HexByte(regNo) + " Value: 0x" + HexByte(readData) + "\r\n");
			} else {
				usb->SendString("Invalid register\r\n");
			}
		}

	} else if (comCmd.compare(0, 9, "writei2c:") == 0) {			// write i2c register
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

	} else if (comCmd.compare("i2cscan\n") == 0) {				// Scan i2c addresses
		if (hidService.JoystickNotifications) {
			usb->SendString("Currently connected\r\n");
		} else {
			usb->SendString("Starting scan ...\n");
			int result = gyro.ScanAddresses();
			usb->SendString("Finished scan. Found " + std::to_string(result) + "\n");
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

