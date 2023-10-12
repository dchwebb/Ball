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
bool usbStarted = false;

int main(void)
{
	InitClocks();					// Set system clocks
	InitHardware();					// Initialise HSEM, IPCC, RTC, EXTI
	config.RestoreConfig();			// Restore config settings from flash
	gyro.Setup();					// Setup address and settings for gyroscope
	APPE_Init();					// Initialise low level BLE functions and schedule start of BLE

	while (1) {
		if (!bleApp.coprocessorFailure) {
			RunPendingTasks();
		}
		usb.cdc.ProcessCommand();	// Check for incoming CDC commands
		basService.TimedRead();		// Updates battery level every few seconds if changed
		if (basService.level == 100 && !usbStarted) {
			usb.InitUSB();			// Start usb only when powered by USB
			usbStarted = true;
		}
		led.Update();				// Check if LED needs to be updated (flashing etc)
	}
}


