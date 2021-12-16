#pragma once
#include "stm32wb55xx.h"

#define SYSTICK 1000						// 1ms
#define USEDONGLE true

void InitPWMTimer();
void InitHardware();
void SystemClock_Config();
