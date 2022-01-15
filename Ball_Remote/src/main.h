#pragma once

//#ifdef __cplusplus
//extern "C" {
//#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32wbxx_hal.h"
#include "app_conf.h"
#include "app_entry.h"
#include "app_common.h"

void Error_Handler(void);

#define BUTTON_SW1_Pin GPIO_PIN_4
#define BUTTON_SW1_GPIO_Port GPIOC
#define BUTTON_SW1_EXTI_IRQn EXTI4_IRQn
#define BUTTON_SW2_Pin GPIO_PIN_0
#define BUTTON_SW2_GPIO_Port GPIOD
#define BUTTON_SW3_Pin GPIO_PIN_1
#define BUTTON_SW3_GPIO_Port GPIOD

#define GREEN_LED_Pin GPIO_PIN_0
#define GREEN_LED_GPIO_Port GPIOB
#define RED_LED_Pin GPIO_PIN_1
#define RED_LED_GPIO_Port GPIOB
#define LD1_Pin GPIO_PIN_5
#define LD1_GPIO_Port GPIOB

#define STLINK_RX_Pin GPIO_PIN_6
#define STLINK_RX_GPIO_Port GPIOB
#define STLINK_TX_Pin GPIO_PIN_7
#define STLINK_TX_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */
//
//#ifdef __cplusplus
//}
//#endif
