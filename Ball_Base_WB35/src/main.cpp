#include "initialisation.h"
#include "app_entry.h"
#include "USB.h"
#include "app_ble.h"


volatile uint32_t SysTickVal = 0;
extern uint32_t SystemCoreClock;

int main(void)
{
	SystemClock_Config();			// Set system clocks
	SystemCoreClockUpdate();		// Read configured clock speed into SystemCoreClock (system clock frequency)

	InitHardware();					// Initialise HSEM, IPCC, RTC, EXTI
	usb.InitUSB();
	APPE_Init();					// Initialise low level BLE functions and schedule start of BLE in while loop

	InitPWMTimer();					// Initialise PWM output

	while (1) {
		if (!bleApp.coprocessorFailure) {
			MX_APPE_Process();
		}
		usb.cdc.ProcessCommand();	// Check for incoming CDC commands
	}
}







