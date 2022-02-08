#include "main.h"
//#include "initialisation.h"
#include "uartHandler.h"
#include "app_entry.h"
#include "USB.h"
#include "SerialHandler.h"
#include "app_ble.h"

RTC_HandleTypeDef hrtc;
SerialHandler serial(usb);

extern uint32_t SystemCoreClock;

int main(void)
{
	SystemClock_Config();			// Set system clocks
	SystemCoreClockUpdate();		// Read configured clock speed into SystemCoreClock (system clock frequency)

	InitHardware();					// Initialise HSEM, IPCC, RTC, EXTI
	InitUart();						// Debugging via STLink UART
	usb.InitUSB();
	APPE_Init();					// Initialise low level BLE functions and schedule start of BLE in while loop

	InitPWMTimer();					// Initialise PWM output

	while (1) {
		MX_APPE_Process();
		serial.Command();			// Check for incoming CDC commands
	}
}

void Error_Handler()
{
	__disable_irq();
	while (1)
	{
	}
}





