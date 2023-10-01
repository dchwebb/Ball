#include "initialisation.h"
#include "BasService.h"
#include "HidService.h"
#include "app_entry.h"
#include "USB.h"
#include "gyroSPI.h"
#include "led.h"

/* TODO
 * Shutdown if advertising exceeds time limit
 *
 */

volatile uint32_t SysTickVal;

int main(void)
{
	InitClocks();					// Set system clocks
	InitHardware();					// Initialise HSEM, IPCC, RTC, EXTI
	usb.InitUSB();
	APPE_Init();					// Initialise low level BLE functions and schedule start of BLE
	gyro.Setup();					// Setup address and settings for gyroscope


	while (1) {
		RunPendingTasks();			// Run any tasks pending in the sequencer
		usb.cdc.ProcessCommand();	// Check for incoming CDC commands
		basService.TimedRead();		// Updates battery level every few seconds if changed
		led.Update();				// Check if LED needs to be updated (flashing etc)
	}
}


