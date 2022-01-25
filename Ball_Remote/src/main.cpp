#include "main.h"
#include "stm32_seq.h"
#include "ble_types.h"
#include "hids_app.h"
#include "bas_app.h"
#include "initialisation.h"
#include "app_entry.h"
#include "uartHandler.h"
#include "compassHandler.h"

void Button1_Task(void) {
	printf("Button 1 pressed\r\n");
	hidService.JoystickNotification(5, 0, 0);
}
void Button2_Task(void) {
	printf("Button 2 pressed\r\n");
	static uint8_t battery = 50;

	BAS_App_Set_Level(battery++);
}

bool sleep = false;

int main(void)
{
	SystemClock_Config();			// Set system clocks
	SystemCoreClockUpdate();		// Read configured clock speed into SystemCoreClock (system clock frequency)

	InitHardware();					// Initialise HSEM, IPCC, RTC, EXTI
	InitUart();						// Debugging via STLink UART
	APPE_Init();					// Initialise low level BLE functions and schedule start of BLE in while loop
	compass.Setup();				// Setup address and settings for magnetometer

	UTIL_SEQ_RegTask(1 << CFG_TASK_SW1_BUTTON_PUSHED_ID, 0, Button1_Task);
	UTIL_SEQ_RegTask(1 << CFG_TASK_SW2_BUTTON_PUSHED_ID, 0, Button2_Task);

	while (1) {
		HAL_GPIO_WritePin(BLUE_LED_GPIO_Port, BLUE_LED_Pin, GPIO_PIN_SET);
		MX_APPE_Process();
		uartCommand();
	}
}


void Error_Handler()
{
	__disable_irq();
	while (1) {
	}
}
