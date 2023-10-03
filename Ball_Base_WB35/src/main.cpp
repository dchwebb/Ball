#include "initialisation.h"
#include "app_entry.h"
#include "app_ble.h"
#include "USB.h"
#include "configManager.h"

// FIXME - LED keeps flashing if trying to connect with no remote module available

volatile uint32_t SysTickVal = 0;
extern uint32_t SystemCoreClock;

int main()
{
	SystemClock_Config();			// Set system clocks
	SystemCoreClockUpdate();		// Read configured clock speed into SystemCoreClock (system clock frequency)

	InitHardware();					// Initialise HSEM, IPCC, RTC, EXTI
	usb.InitUSB();
	APPE_Init();					// Initialise low level BLE functions and schedule start of BLE in while loop
	configManager.RestoreConfig();
	InitPWMTimer();					// Initialise PWM output

	while (1) {
		if (!bleApp.coprocessorFailure) {
			RunPendingTasks();
		}
		usb.cdc.ProcessCommand();	// Check for incoming CDC commands
		bleApp.LedFlash();			// Check if connection LED needs to be flashed
	}
}







