#pragma once

//#include "stm32wbxx_hal.h"
//#include "app_conf.h"
#include "app_common.h"
#include "initialisation.h"

#ifdef USEDONGLE
#define LED_GREEN_Pin GPIO_PIN_0
#define LED_GREEN_GPIO_Port GPIOB
#define LED_RED_Pin GPIO_PIN_1
#define LED_RED_GPIO_Port GPIOB
#define LED_BLUE_Pin GPIO_PIN_4
#define LED_BLUE_GPIO_Port GPIOA
#else
#define LED_GREEN_Pin GPIO_PIN_0
#define LED_GREEN_GPIO_Port GPIOB
#define LED_RED_Pin GPIO_PIN_1
#define LED_RED_GPIO_Port GPIOB
#define LED_BLUE_Pin GPIO_PIN_5
#define LED_BLUE_GPIO_Port GPIOB
#endif

//#define STLINK_RX_Pin GPIO_PIN_6
//#define STLINK_RX_GPIO_Port GPIOB
//#define STLINK_TX_Pin GPIO_PIN_7
//#define STLINK_TX_GPIO_Port GPIOB


void Error_Handler();
