#pragma once
#include "stm32wbxx.h"

#define SYSTICK 1000						// 1ms
void InitHardware();
void InitClocks();
void RTCInterrupt(uint32_t seconds);
