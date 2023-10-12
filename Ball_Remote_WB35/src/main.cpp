#include "initialisation.h"
#include "BleApp.h"
#include "BasService.h"
#include "HidService.h"
#include "app_entry.h"
#include "USB.h"
#include "gyroSPI.h"
#include "led.h"
#include "configManager.h"

/* TODO
 * Test USB wake up and sleep
 * Test sleep clock settings
 *
 */

volatile uint32_t SysTickVal;

int main(void)
{
	InitClocks();					// Set system clocks
	InitHardware();					// Initialise HSEM, IPCC, RTC, EXTI
	config.RestoreConfig();
	gyro.Setup();					// Setup address and settings for gyroscope
	APPE_Init();					// Initialise low level BLE functions and schedule start of BLE
//	usb.InitUSB();


	while (1) {
		if (!bleApp.coprocessorFailure) {
			RunPendingTasks();
		}
		usb.cdc.ProcessCommand();	// Check for incoming CDC commands
		basService.TimedRead();		// Updates battery level every few seconds if changed
		led.Update();				// Check if LED needs to be updated (flashing etc)
	}
}


