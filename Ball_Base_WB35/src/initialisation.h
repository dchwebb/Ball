#pragma once
#include "stm32wb35xx.h"

#define SYSTICK 1000						// 1ms
#define USEDONGLE true

extern "C" {
extern volatile uint32_t uwTick;
}

void InitPWMTimer();
void InitHardware();
void SystemClock_Config();
