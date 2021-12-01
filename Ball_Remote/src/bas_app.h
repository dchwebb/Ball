#pragma once

#include "svc_ctl.h"

extern "C" {		// Declare with C linkage or will be overridden by weak declaration in svc_ctl.c
void BAS_Init();
}

tBleStatus BAS_Update_Char();
void BAS_App_Init();
void BAS_App_Set_Level(uint8_t level);
void BAS_App_Send_Notification();

