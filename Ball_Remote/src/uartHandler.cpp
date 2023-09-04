#include "uartHandler.h"
#include "gyroSPI.h"
#include "app_ble.h"
#include "hids_app.h"
extern "C" {
#include "shci.h"
#include "stm32_lpm.h"
#include "stm32_seq.h"
#include "app_conf.h"
}
#include <charconv>

volatile uint8_t uartCmdPos = 0;
volatile char uartCmd[100];
volatile bool uartCmdRdy = false;

// Manages communication to ST Link debugger UART
void InitUart() {
	// MODER 00: Input mode, 01: General purpose output mode, 10: Alternate function mode, 11: Analog mode (reset state)

    // PB6: USART1_TX; PB7 USART1_RX
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

	GPIOB->MODER &= ~GPIO_MODER_MODE6_0;			// Set alternate function on PB6
	GPIOB->AFR[0] |= 7 << GPIO_AFRL_AFSEL6_Pos;		// Alternate function for USART1_TX is AF7
	GPIOB->MODER &= ~GPIO_MODER_MODE7_0;			// Set alternate function on PB7
	GPIOB->AFR[0] |= 7 << GPIO_AFRL_AFSEL7_Pos;		// Alternate function for USART1_RX is AF7

	// By default clock source is muxed to peripheral clock 2 which is system clock (change clock source in RCC->CCIPR1)
	// Calculations depended on oversampling mode set in CR1 OVER8. Default = 0: Oversampling by 16
	int USARTDIV = (SystemCoreClock * 2) / 115200;		// clk / desired_baud
	USART1->BRR = USARTDIV & ~8;					// BRR[3] must not be set set to 0x226
	USART1->CR1 &= ~USART_CR1_M;					// 0: 1 Start bit, 8 Data bits, n Stop bit; 	1: 1 Start bit, 9 Data bits, n Stop bit
	USART1->CR1 |= USART_CR1_RE;					// Receive enable
	USART1->CR1 |= USART_CR1_TE;					// Transmitter enable
	USART1->CR1 |= USART_CR1_OVER8;					// Oversampling by 8 (default is 16)

	// Set up interrupts
	USART1->CR1 |= USART_CR1_RXNEIE;
	NVIC_SetPriority(USART1_IRQn, 3);				// Lower is higher priority
	NVIC_EnableIRQ(USART1_IRQn);

	USART1->CR1 |= USART_CR1_UE;					// USART Enable

}


std::string IntToString(const int32_t& v) {
	return std::to_string(v);
}

std::string HexToString(const uint32_t& v, const bool& spaces) {
	char buf[20];
	if (spaces) {
		if (v != 0) {
			sprintf(buf, "%02luX %02luX %02luX %02luX", v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF);
		} else {
			sprintf(buf, " ");
		}
	} else {
		sprintf(buf, "%luX", v);
	}
	return std::string(buf);

}

std::string HexByte(const uint16_t& v) {
	char buf[50];
	sprintf(buf, "%X", v);
	return std::string(buf);

}

void uartSendChar(char c) {
	while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0);
	USART1->TDR = c;
}


size_t uartSendString(const unsigned char* s, size_t size) {
	for (size_t i = 0; i < size; ++i) {
		while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0);
		USART1->TDR = s[i];
	}
	return size;
}

void uartSendString(const char* s) {
	char c = s[0];
	uint16_t i = 0;
	while (c) {
		while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0);
		USART1->TDR = c;
		c = s[++i];
	}
}

void uartSendString(const std::string& s) {
	for (char c : s) {
		while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0);
		USART1->TDR = c;
	}
}




// Check if a command has been received from USB, parse and action as required
bool uartCommand()
{
	//char buf[50];

	if (!uartCmdRdy) {
		return false;
	}

	std::string_view comCmd{(const char*)uartCmd};

	if (comCmd.compare("help\n") == 0) {

		uartSendString("Mountjoy Ball Remote\r\n"
				"\r\nSupported commands:\r\n"
				"readspi:HH      -  Read I2C register at 0xHH\r\n"
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
				"\r\n");

	} else if (comCmd.compare("outputgyro\n") == 0) {					// Output raw gyro data
		hidService.outputGyro = !hidService.outputGyro;


	} else if (comCmd.compare("gyroread\n") == 0) {					// Trigger a repeated read
		if (hidService.JoystickNotifications) {
			printf("Currently connected\r\n");
		} else {
			gyro.DebugRead();
			printf("x: %d, y:%d, z: %d\n", gyro.gyroData.x, gyro.gyroData.y, gyro.gyroData.z);
		}

	} else if (comCmd.compare("contread\n") == 0) {					// Trigger a repeated read
		gyro.ContinualRead();
		printf("x: %d, y:%d, z: %d\n", gyro.gyroData.x, gyro.gyroData.y, gyro.gyroData.z);


	} else if (comCmd.compare(0, 8, "readspi:") == 0) {				// Read spi register
		if (hidService.JoystickNotifications) {
			printf("Currently connected\r\n");
		} else {

			uint8_t regNo;
			auto res = std::from_chars(comCmd.data() + comCmd.find(":") + 1, comCmd.data() + comCmd.size(), regNo, 16);

			if (res.ec == std::errc()) {
				uint8_t readData = gyro.ReadData(regNo);
				uartSendString("I2C Register: 0x" + HexByte(regNo) + " Value: 0x" + HexByte(readData) + "\r\n");
			} else {
				printf("Invalid register\r\n");
			}
		}

	} else if (comCmd.compare(0, 8, "readi2c:") == 0) {				// Read i2c register
		if (hidService.JoystickNotifications) {
			printf("Currently connected\r\n");
		} else {

			uint8_t regNo;
			auto res = std::from_chars(comCmd.data() + comCmd.find(":") + 1, comCmd.data() + comCmd.size(), regNo, 16);

			if (res.ec == std::errc()) {
				uint8_t readData = gyro.ReadData(regNo);
				uartSendString("SPI Register: 0x" + HexByte(regNo) + " Value: 0x" + HexByte(readData) + "\r\n");
			} else {
				uartSendString("Invalid register\r\n");
			}
		}

	} else if (comCmd.compare(0, 9, "writei2c:") == 0) {			// write i2c register
		if (hidService.JoystickNotifications) {
			printf("Currently connected\r\n");
		} else {

			uint8_t regNo, value;
			auto res = std::from_chars(comCmd.data() + comCmd.find(":") + 1, comCmd.data() + comCmd.size(), regNo, 16);

			if (res.ec == std::errc()) {			// no error
				auto res = std::from_chars(comCmd.data() + comCmd.find(",") + 1, comCmd.data() + comCmd.size(), value, 16);
				if (res.ec == std::errc()) {			// no error
					gyro.WriteCmd(regNo, value);
					uartSendString("I2C write: Register: 0x" + HexByte(regNo) + " Value: 0x" + HexByte(value) + "\r\n");
				} else {
					uartSendString("Invalid value\r\n");
				}
			} else {
				uartSendString("Invalid register\r\n");
			}
		}
/*
	} else if (comCmd.compare("i2cscan\n") == 0) {				// Scan i2c addresses
		if (hidService.JoystickNotifications) {
			printf("Currently connected\r\n");
		} else {
			uartSendString("Starting scan ...\n");
			int result = gyro.ScanAddresses();
			printf("Finished scan. Found %d\n", result);
		}
*/
	} else if (comCmd.compare("fwversion\n") == 0) {			// Version of BLE firmware
		WirelessFwInfo_t fwInfo;
		if (SHCI_GetWirelessFwInfo(&fwInfo) == 0) {
			printf("BLE firmware version: %d.%d.%d.%d; FUS version: %d.%d.%d\r\n",
					fwInfo.VersionMajor, fwInfo.VersionMinor, fwInfo.VersionSub, fwInfo.VersionBranch,
					fwInfo.FusVersionMajor, fwInfo.FusVersionMinor, fwInfo.FusVersionSub);
		}

	} else if (comCmd.compare("sleep\n") == 0) {				// Enter sleep mode
		uartSendString("Going to sleep\n");
		extern bool sleep;
		sleep = true;		// Triggers idle routine UTIL_SEQ_Idle() in app_entry.c

	} else if (comCmd.compare("shutdown\n") == 0) {				// Enter sleep mode
		uartSendString("Shutting down\n");
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
		uartSendString("Unrecognised command: " + std::string(comCmd) + "Type 'help' for supported commands\r\n");
	}

	uartCmdRdy = false;
	return true;
}


extern "C" {

// USART Decoder
void USART1_IRQHandler() {
	// Check for overrun
	if ((USART1->ISR & USART_ISR_ORE) == USART_ISR_ORE) {
		USART1->ICR |= USART_ICR_ORECF;
	}

	if (!uartCmdRdy) {
		uartCmd[uartCmdPos] = USART1->RDR; 				// accessing RDR automatically resets the receive flag
		if (uartCmd[uartCmdPos] == 10) {
			uartCmd[uartCmdPos + 1] = 0;				// ensure last character is null terminator
			uartCmdRdy = true;
			uartCmdPos = 0;
		} else {
			uartCmdPos++;
		}
	}
}

//void SerialHandler::Handler(uint8_t* data, uint32_t length)
//{
//	static bool newCmd = true;
//	if (newCmd) {
//		ComCmd = std::string(reinterpret_cast<char*>(data), length);
//		newCmd = false;
//	} else {
//		ComCmd.append(reinterpret_cast<char*>(data), length);
//	}
//	if (*ComCmd.rbegin() == '\r')
//		*ComCmd.rbegin() = '\n';
//
//	if (*ComCmd.rbegin() == '\n') {
//		CmdPending = true;
//		newCmd = true;
//	}
//
//}

// To enable UART for printf commands
size_t _write(int handle, const unsigned char* buf, size_t bufSize)
{
	return uartSendString(buf, bufSize);
}
}
