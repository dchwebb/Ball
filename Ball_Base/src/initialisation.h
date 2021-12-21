#pragma once
#include "stm32wb55xx.h"

#define SYSTICK 1000						// 1ms
#define USEDONGLE false

extern "C" {
extern volatile uint32_t uwTick;
}

void InitPWMTimer();
void InitHardware();
void SystemClock_Config();
