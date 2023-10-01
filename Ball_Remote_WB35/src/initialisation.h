#pragma once
#include "stm32wbxx.h"

#define SYSTICK 1000						// 1ms
//#define USEBASEBOARD 1						// Temporarily set to true to test code using Base hardware
void InitHardware();
void InitClocks();
