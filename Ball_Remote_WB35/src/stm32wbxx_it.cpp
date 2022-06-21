#include "main.h"
#include "app_common.h"
#include "stm32_seq.h"
#include "gyroHandler.h"
#include "USB.h"

extern "C" {
void SysTick_Handler() {
	uwTick++;
	//HAL_IncTick();
}

void USB_LP_IRQHandler() {
	usb.USBInterruptHandler();
}

void EXTI4_IRQHandler() {
	EXTI->PR1 = EXTI_PR1_PIF4;
	UTIL_SEQ_SetTask(1 << CFG_TASK_SW1_BUTTON_PUSHED_ID, CFG_SCH_PRIO_0);
}

void I2C1_EV_IRQHandler() {
	I2C1->CR2 |= I2C_CR2_STOP;			// Clear the Transfer complete interrupt (STOP bit cleared when next START issued)
	gyro.ProcessResults();
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
