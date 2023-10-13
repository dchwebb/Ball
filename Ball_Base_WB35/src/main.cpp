#include "initialisation.h"
#include "BleApp.h"
#include "app_entry.h"
#include "USB.h"
#include "configManager.h"
#include "led.h"
#include "HidApp.h"

// FIXME - LED keeps flashing if trying to connect with no remote module available

volatile uint32_t SysTickVal = 0;
extern uint32_t SystemCoreClock;

int main()
{
	InitClocks();					// Set system clocks
	SystemCoreClockUpdate();		// Read configured clock speed into SystemCoreClock (system clock frequency)

	InitHardware();					// Initialise HSEM, IPCC, RTC, EXTI
	usb.InitUSB();
	APPE_Init();					// Initialise low level BLE functions and schedule start of BLE in while loop
	config.RestoreConfig();
	InitPWMTimer();					// Initialise PWM output

	while (1) {
		if (!bleApp.coprocessorFailure) {
			RunPendingTasks();
		}
		usb.cdc.ProcessCommand();	// Check for incoming CDC commands
		led.Update();				// Check if connection LED needs to be flashed
		hidApp.CheckBattery();		// Run a periodic check to get battery level from remote if sleeping
	}
}







