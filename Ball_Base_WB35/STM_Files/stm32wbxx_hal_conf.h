#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#define HAL_MODULE_ENABLED
//#define HAL_RTC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED

#if !defined  (HSE_VALUE)
#define HSE_VALUE    32000000U
#endif

#if !defined  (LSE_VALUE)
#define LSE_VALUE    32768U
#endif

#include "stm32wbxx_hal_cortex.h"
//#include "stm32wbxx_hal_rtc.h"

#define assert_param(expr) ((void)0U)


#ifdef __cplusplus
}
#endif

