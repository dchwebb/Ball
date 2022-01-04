#include "main.h"
#include "initialisation.h"
#include "stm32wbxx_it.h"
#include "app_ble.h"
#include "USB.h"

//extern IPCC_HandleTypeDef hipcc;
//extern DMA_HandleTypeDef hdma_lpuart1_tx;
//extern DMA_HandleTypeDef hdma_usart1_tx;
//extern UART_HandleTypeDef hlpuart1;
//extern UART_HandleTypeDef huart1;

extern "C" {

void SysTick_Handler() {
	HAL_IncTick();
}

extern USBHandler usb;

void USB_LP_IRQHandler() {
	usb.USBInterruptHandler();
}

#if USEDONGLE
void EXTI15_10_IRQHandler() {
	EXTI->PR1 = EXTI_PR1_PIF10;
	APP_BLE_Scan_and_Connect();
}
#else
void EXTI0_IRQHandler() {
	EXTI->PR1 = EXTI_PR1_PIF0;
	//APP_BLE_Key_Button2_Action();
	usb.OutputDebug();
}

void EXTI1_IRQHandler() {
	EXTI->PR1 = EXTI_PR1_PIF1;
	APP_BLE_Key_Button3_Action();
}

void EXTI4_IRQHandler() {
	EXTI->PR1 = EXTI_PR1_PIF4;
	bleApp.ScanAndConnect();
}
#endif

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



