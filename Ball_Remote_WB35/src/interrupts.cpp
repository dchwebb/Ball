#include "main.h"
#include "app_common.h"
#include "stm32_seq.h"
#include "gyroSPI.h"
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
	// FIXME Button pressed
}


void TIM2_IRQHandler() {
	TIM2->SR &= ~TIM_SR_UIF;
	gyro.OutputGyro();

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
