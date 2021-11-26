#include "main.h"
#include "initialisation.h"
#include "app_entry.h"

//UART_HandleTypeDef hlpuart1;
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_lpuart1_tx;
DMA_HandleTypeDef hdma_usart1_tx;
RTC_HandleTypeDef hrtc;

extern uint32_t SystemCoreClock;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);


int main(void)
{
	SystemClock_Config();
	SystemCoreClockUpdate();		// Update SystemCoreClock (system clock frequency)

	InitHardware();

	//MX_DMA_Init();
	//MX_LPUART1_UART_Init();

	APPE_Init();
	MX_GPIO_Init();
	MX_USART1_UART_Init();
	InitTimer();

	while (1) {
		MX_APPE_Process();
	}
}



void MX_USART1_UART_Init(void)
{
	// DMA controller clock enable
	__HAL_RCC_DMAMUX1_CLK_ENABLE();
	__HAL_RCC_DMA2_CLK_ENABLE();
//	__HAL_RCC_DMA1_CLK_ENABLE();

//	// DMA1_Channel4_IRQn interrupt configuration
//	HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 15, 0);
//	HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);

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



//static void MX_DMA_Init(void)
//{
//	// DMA controller clock enable
//	__HAL_RCC_DMAMUX1_CLK_ENABLE();
//	__HAL_RCC_DMA2_CLK_ENABLE();
////	__HAL_RCC_DMA1_CLK_ENABLE();
//
////	// DMA1_Channel4_IRQn interrupt configuration
////	HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 15, 0);
////	HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
//
//	// DMA2_Channel4_IRQn interrupt configuration
//	HAL_NVIC_SetPriority(DMA2_Channel4_IRQn, 15, 0);
//	HAL_NVIC_EnableIRQ(DMA2_Channel4_IRQn);
//}


static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// GPIO Ports Clock Enable
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	// Configure GPIO pin Output Level
	HAL_GPIO_WritePin(GPIOB, LED_GREEN_Pin|LED_RED_Pin|LED_BLUE_Pin, GPIO_PIN_RESET);

	// Configure GPIO pin : BUTTON_SW1_Pin
	GPIO_InitStruct.Pin = BUTTON_SW1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(BUTTON_SW1_GPIO_Port, &GPIO_InitStruct);

	// Configure GPIO pins : LED_GREEN_Pin LED_RED_Pin LED_BLUE_Pin
	GPIO_InitStruct.Pin = LED_GREEN_Pin|LED_RED_Pin|LED_BLUE_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	// Configure GPIO pins : BUTTON_SW2_Pin BUTTON_SW3_Pin
	GPIO_InitStruct.Pin = BUTTON_SW2_Pin|BUTTON_SW3_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	// EXTI interrupt init
	HAL_NVIC_SetPriority(EXTI0_IRQn, 3, 0);
	HAL_NVIC_EnableIRQ(EXTI0_IRQn);

	HAL_NVIC_SetPriority(EXTI1_IRQn, 3, 0);
	HAL_NVIC_EnableIRQ(EXTI1_IRQn);

	HAL_NVIC_SetPriority(EXTI4_IRQn, 3, 0);
	HAL_NVIC_EnableIRQ(EXTI4_IRQn);
}


void Error_Handler()
{
	__disable_irq();
	while (1)
	{
	}
}
