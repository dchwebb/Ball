#include "main.h"
#include "stm32_seq.h"
#include "ble_types.h"
#include "hids_app.h"
#include "bas_app.h"
#include "initialisation.h"
#include "app_entry.h"
#include "USB.h"
#include "gyroSPI.h"


bool sleep = false;


int main(void)
{
	SystemClock_Config();			// Set system clocks
	SystemCoreClockUpdate();		// Read configured clock speed into SystemCoreClock (system clock frequency)

	InitHardware();					// Initialise HSEM, IPCC, RTC, EXTI
	usb.InitUSB();
	APPE_Init();					// Initialise low level BLE functions and schedule start of BLE

	gyro.Setup();					// Setup address and settings for gyroscope


	while (1) {
		HAL_GPIO_WritePin(BLUE_LED_GPIO_Port, BLUE_LED_Pin, GPIO_PIN_SET);
		MX_APPE_Process();
		usb.cdc.ProcessCommand();			// Check for incoming CDC commands
		basService.TimedRead();		// Updates battery level every few seconds if changed
	}
}


void Error_Handler()
{
	__disable_irq();
	while (1) {
	}
}
