#include "main.h"
#include "stm32_seq.h"
#include "ble_types.h"
#include "hids_app.h"
#include "bas_app.h"
#include "initialisation.h"
#include "app_entry.h"
#include "SerialHandler.h"
#include "USB.h"
#include "gyroSPI.h"

void Button1_Task(void) {
	printf("Button 1 pressed\r\n");
	hidService.JoystickNotification(5, 0, 0);
}
void Button2_Task(void) {
	printf("Button 2 pressed\r\n");
	static uint8_t battery = 50;

	basService.SetLevel(battery++);
}

bool sleep = false;
SerialHandler serial(usb);
bool coprocessorFailure = false;

int main(void)
{
	SystemClock_Config();			// Set system clocks
	SystemCoreClockUpdate();		// Read configured clock speed into SystemCoreClock (system clock frequency)

	InitHardware();					// Initialise HSEM, IPCC, RTC, EXTI
	usb.InitUSB();
	APPE_Init();					// Initialise low level BLE functions and schedule start of BLE in while loop

	gyro.Setup();					// Setup address and settings for gyroscope

	UTIL_SEQ_RegTask(1 << CFG_TASK_SW1_BUTTON_PUSHED_ID, 0, Button1_Task);

	while (1) {
		HAL_GPIO_WritePin(BLUE_LED_GPIO_Port, BLUE_LED_Pin, GPIO_PIN_SET);
		MX_APPE_Process();
		serial.Command();			// Check for incoming CDC commands
	}
}


void Error_Handler()
{
	__disable_irq();
	while (1) {
	}
}
