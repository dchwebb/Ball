#include "main.h"
#include "stm32wbxx_it.h"
#include "app_common.h"

void NMI_Handler(void) {
	while (1) {	}
}

void HardFault_Handler(void) {
	while (1) { }
}

void MemManage_Handler(void) {
	while (1) { }
}

void BusFault_Handler(void){
	while (1) { }
}

void UsageFault_Handler(void) {
	while (1) { }
}

void SVC_Handler(void) {}

void DebugMon_Handler(void) {
	/* USER CODE BEGIN DebugMonitor_IRQn 0 */

	/* USER CODE END DebugMonitor_IRQn 0 */
	/* USER CODE BEGIN DebugMonitor_IRQn 1 */

	/* USER CODE END DebugMonitor_IRQn 1 */
}


void PendSV_Handler(void) { }

void SysTick_Handler(void) {
	HAL_IncTick();
}

void EXTI4_IRQHandler(void) {
	//	UTIL_SEQ_SetTask(1 << CFG_TASK_NOTIFY_TEMPERATURE, CFG_SCH_PRIO_0);
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);
}
void EXTI0_IRQHandler(void) {
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

void HSEM_IRQHandler(void) {
	HAL_HSEM_IRQHandler();
}

void RTC_WKUP_IRQHandler(void) {
	HW_TS_RTC_Wakeup_Handler();
}

void IPCC_C1_RX_IRQHandler(void) {
	HW_IPCC_Rx_Handler();
}

void IPCC_C1_TX_IRQHandler(void) {
	HW_IPCC_Tx_Handler();
}

