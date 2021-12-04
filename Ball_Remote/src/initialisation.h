#pragma once
#include "stm32wbxx.h"

#define SYSTICK 1000						// 1ms

void InitPWMTimer();
void InitHardware();
void SystemClock_Config();
void InitI2C();
