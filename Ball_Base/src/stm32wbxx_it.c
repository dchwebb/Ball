#include "main.h"
#include "stm32wbxx_it.h"

//extern IPCC_HandleTypeDef hipcc;
extern DMA_HandleTypeDef hdma_lpuart1_tx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern UART_HandleTypeDef hlpuart1;
extern UART_HandleTypeDef huart1;


void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
	HAL_IncTick();
}


void EXTI0_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}


void EXTI1_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}

void EXTI4_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);
}


void USART1_IRQHandler(void)
{
	HAL_UART_IRQHandler(&huart1);
}

//void LPUART1_IRQHandler(void)
//{
//	HAL_UART_IRQHandler(&hlpuart1);
//}

void IPCC_C1_RX_IRQHandler(void)
{
	HW_IPCC_Rx_Handler();
	//HAL_IPCC_RX_IRQHandler(&hipcc);
}

void IPCC_C1_TX_IRQHandler(void)
{
	HW_IPCC_Tx_Handler();
	//HAL_IPCC_TX_IRQHandler(&hipcc);
}

void HSEM_IRQHandler(void)
{
	HAL_HSEM_IRQHandler();
}

void DMA2_Channel4_IRQHandler(void)
{
	HAL_DMA_IRQHandler(&hdma_usart1_tx);
}


void NMI_Handler(void) {
	while (1) {}
}
void HardFault_Handler(void) {
	while (1) {}
}
void MemManage_Handler(void) {
	while (1) {}
}
void BusFault_Handler(void) {
	while (1) {}
}
void UsageFault_Handler(void) {
	while (1) {}
}
