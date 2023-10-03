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
	// Gyro wake up
	EXTI->PR1 = EXTI_PR1_PIF9;
}


void TIM2_IRQHandler() {
	TIM2->SR &= ~TIM_SR_UIF;
	GPIOB->ODR |= GPIO_ODR_OD8;
	gyro.OutputGyro();
	GPIOB->ODR &= ~GPIO_ODR_OD8;
}


void RTC_WKUP_IRQHandler() {
	//BleApp::QueueLPAdvertising();
	bleApp.WakeUp();
	RTCInterrupt(0);
	//HW_TS_RTC_Wakeup_Handler();
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
