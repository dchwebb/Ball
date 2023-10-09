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
		bleApp.lowPowerMode = BleApp::LowPowerMode::Shutdown;
		bleApp.sleepState = BleApp::SleepState::RequestSleep;
	}
}


void EXTI9_5_IRQHandler() {
	// 8 is gyro data ready; 9 is wake up from sleep with gyro interrupt (no additional interrupt handling needed)
	if ((EXTI->PR1 & EXTI_PR1_PIF8) == EXTI_PR1_PIF8) {			// Gyro data ready
		gyro.OutputGyro();
	}
	if ((EXTI->PR1 & EXTI_PR1_PIF8) == EXTI_PR1_PIF8) {			// Gyro motion wakeup
		bleApp.motionWakeup = true;
	}
	EXTI->PR1 = EXTI_PR1_PIF;								// Clear interrupts
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
