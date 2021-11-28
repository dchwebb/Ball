#pragma once

//#include "stm32wbxx_hal.h"
//#include "app_conf.h"
#include "app_common.h"

//#define BUTTON_SW1_Pin GPIO_PIN_4
//#define BUTTON_SW1_GPIO_Port GPIOC
//#define BUTTON_SW1_EXTI_IRQn EXTI4_IRQn
#define LED_GREEN_Pin GPIO_PIN_0
#define LED_GREEN_GPIO_Port GPIOB
#define LED_RED_Pin GPIO_PIN_1
#define LED_RED_GPIO_Port GPIOB
//#define JTMS_Pin GPIO_PIN_13
//#define JTMS_GPIO_Port GPIOA
//#define JTCK_Pin GPIO_PIN_14
//#define JTCK_GPIO_Port GPIOA
//#define BUTTON_SW2_Pin GPIO_PIN_0
//#define BUTTON_SW2_GPIO_Port GPIOD
//#define BUTTON_SW2_EXTI_IRQn EXTI0_IRQn
//#define BUTTON_SW3_Pin GPIO_PIN_1
//#define BUTTON_SW3_GPIO_Port GPIOD
//#define BUTTON_SW3_EXTI_IRQn EXTI1_IRQn
//#define JTDO_Pin GPIO_PIN_3
//#define JTDO_GPIO_Port GPIOB
#define LED_BLUE_Pin GPIO_PIN_5
#define LED_BLUE_GPIO_Port GPIOB
#define STLINK_RX_Pin GPIO_PIN_6
//#define STLINK_RX_GPIO_Port GPIOB
#define STLINK_TX_Pin GPIO_PIN_7
//#define STLINK_TX_GPIO_Port GPIOB

void MX_USART1_UART_Init(void);
void Error_Handler();
