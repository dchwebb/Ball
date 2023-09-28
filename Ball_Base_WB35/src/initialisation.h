#pragma once
#include "stm32wb35xx.h"

#define SYSTICK 1000						// 1ms

extern volatile uint32_t SysTickVal;

void InitPWMTimer();
void InitHardware();
void SystemClock_Config();
