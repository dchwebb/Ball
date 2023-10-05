#include <BleApp.h>
#include "initialisation.h"
#include "USB.h"


extern "C" {

void SysTick_Handler() {
	++SysTickVal;
}

void USB_LP_IRQHandler() {
	usb.USBInterruptHandler();
}

void EXTI4_IRQHandler() {
	// Handle connect button press
	EXTI->PR1 = EXTI_PR1_PIF4;
	bleApp.SwitchConnectState();
}

void IPCC_C1_RX_IRQHandler() {
	HW_IPCC_Rx_Handler();
}

void IPCC_C1_TX_IRQHandler() {
	HW_IPCC_Tx_Handler();
}

void HSEM_IRQHandler() {
//	HAL_HSEM_IRQHandler();
}

void NMI_Handler() {
	while (1) {}
}
void HardFault_Handler() {
	while (1) {}
}
void MemManage_Handler() {
	while (1) {}
}
void BusFault_Handler() {
	while (1) {}
}
void UsageFault_Handler() {
	while (1) {}
}

void SVC_Handler() {}

void DebugMon_Handler() {}

void PendSV_Handler() {}

}

