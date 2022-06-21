#include "main.h"
#include "initialisation.h"
#include "app_ble.h"
#include "USB.h"


extern "C" {

void SysTick_Handler() {
	HAL_IncTick();
}

void USB_LP_IRQHandler() {
	usb.USBInterruptHandler();
}

void EXTI4_IRQHandler() {
	EXTI->PR1 = EXTI_PR1_PIF4;
	if (bleApp.deviceConnectionStatus != BleApp::ConnectionStatus::ClientConnected)	{
		bleApp.ScanAndConnect();
	} else {
		bleApp.DisconnectRequest();
	}
}

void IPCC_C1_RX_IRQHandler() {
	HW_IPCC_Rx_Handler();
}

void IPCC_C1_TX_IRQHandler() {
	HW_IPCC_Tx_Handler();
}

void HSEM_IRQHandler() {
	HAL_HSEM_IRQHandler();
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

//void USART1_IRQHandler()
//{
//	HAL_UART_IRQHandler(&huart1);
//}

//void LPUART1_IRQHandler()
//{
//	HAL_UART_IRQHandler(&hlpuart1);
//}

//void DMA2_Channel4_IRQHandler()
//{
//	HAL_DMA_IRQHandler(&hdma_usart1_tx);
//}

}



