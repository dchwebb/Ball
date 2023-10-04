#include "app_common.h"
#include "gyroSPI.h"
#include "USB.h"
#include "BleApp.h"

extern "C" {
void SysTick_Handler() {
	++SysTickVal;
}

void USB_LP_IRQHandler() {
	usb.USBInterruptHandler();
}

void EXTI4_IRQHandler() {
	// Connect Button pressed (used for sleeping/wakeup)
	EXTI->PR1 = EXTI_PR1_PIF4;
	if (bleApp.sleepState == BleApp::SleepState::Awake) {
		bleApp.lowPowerMode = BleApp::LowPowerMode::Sleep;
		bleApp.sleepState = BleApp::SleepState::RequestSleep;
	}
}


void EXTI9_5_IRQHandler() {

	if ((EXTI->PR1 & EXTI_PR1_PIF8) == EXTI_PR1_PIF8) {			// Gyro data ready
		EXTI->PR1 = EXTI_PR1_PIF8;								// Clear interrupt
		//GPIOB->ODR |= GPIO_ODR_OD8;
		gyro.OutputGyro();
		//GPIOB->ODR &= ~GPIO_ODR_OD8;
	} else if ((EXTI->PR1 & EXTI_PR1_PIF9) == EXTI_PR1_PIF9){		// Gyro wake up
		EXTI->PR1 = EXTI_PR1_PIF9;								// Clear interrupt
	}
}


void TIM2_IRQHandler() {
	TIM2->SR &= ~TIM_SR_UIF;
	GPIOB->ODR |= GPIO_ODR_OD8;
	gyro.OutputGyro();
	GPIOB->ODR &= ~GPIO_ODR_OD8;
}


void RTC_WKUP_IRQHandler() {
	bleApp.RTCWakeUp();
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
