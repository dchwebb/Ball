#pragma once
#include "stm32wb55xx.h"

#define SYSTICK 1000						// 1ms

void InitTimer();
void InitHardware();
void SystemClock_Config();
