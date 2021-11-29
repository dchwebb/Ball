#include "main.h"
#include "stm32wbxx_it.h"
#include "app_common.h"
#include "stm32_seq.h"

extern "C" {
void SysTick_Handler() {
	HAL_IncTick();
}

void EXTI4_IRQHandler() {
	EXTI->PR1 = EXTI_PR1_PIF4;
	UTIL_SEQ_SetTask(1 << CFG_TASK_SW1_BUTTON_PUSHED_ID, CFG_SCH_PRIO_0);
//	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);
}

void EXTI0_IRQHandler() {
	EXTI->PR1 = EXTI_PR1_PIF0;
	UTIL_SEQ_SetTask(1 << CFG_TASK_SW2_BUTTON_PUSHED_ID, CFG_SCH_PRIO_0);
	//HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

void EXTI1_IRQHandler(void) {
	EXTI->PR1 = EXTI_PR1_PIF1;
	printf("Button 3 ...\r\n");
}

void HSEM_IRQHandler() {
	HAL_HSEM_IRQHandler();
}

void RTC_WKUP_IRQHandler() {
	HW_TS_RTC_Wakeup_Handler();
}

void IPCC_C1_RX_IRQHandler() {
	HW_IPCC_Rx_Handler();
}

void IPCC_C1_TX_IRQHandler() {
	HW_IPCC_Tx_Handler();
}

void NMI_Handler() {
	while (1) {	}
}

void HardFault_Handler() {
	while (1) { }
}

void MemManage_Handler() {
	while (1) { }
}

void BusFault_Handler(){
	while (1) { }
}

void UsageFault_Handler() {
	while (1) { }
}

void SVC_Handler() {}

void DebugMon_Handler() {}

void PendSV_Handler() {}

}
