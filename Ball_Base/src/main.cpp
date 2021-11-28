#include "main.h"
#include "initialisation.h"
#include "uartHandler.h"
#include "app_entry.h"

RTC_HandleTypeDef hrtc;

extern uint32_t SystemCoreClock;

int main(void)
{
	SystemClock_Config();			// Set system clocks
	SystemCoreClockUpdate();		// Read configured clock speed into SystemCoreClock (system clock frequency)

	InitHardware();					// Initialise HSEM, IPCC, RTC, EXTI
	InitUart();						// Debugging via STLink UART
	APPE_Init();					// Initialise low level BLE functions and schedule start of BLE in while loop

	InitPWMTimer();					// Initialise PWM output

	while (1) {
		MX_APPE_Process();
	}
}

void Error_Handler()
{
	__disable_irq();
	while (1)
	{
	}
}

/*
void MX_USART1_UART_Init(void)
{
	// DMA controller clock enable
	__HAL_RCC_DMAMUX1_CLK_ENABLE();
	__HAL_RCC_DMA2_CLK_ENABLE();

	// DMA2_Channel4_IRQn interrupt configuration
	HAL_NVIC_SetPriority(DMA2_Channel4_IRQn, 15, 0);
	HAL_NVIC_EnableIRQ(DMA2_Channel4_IRQn);


	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_8;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}

	if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) {
		Error_Handler();
	}

	if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) {
		Error_Handler();
	}

	if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK) {
		Error_Handler();
	}
}
*/




