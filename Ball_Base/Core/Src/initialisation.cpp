//#include "initialisation.h"
#include "stm32wb55xx.h"
extern "C"

void InitTimer() {
	// TIM2 Channel 1 Output: *PA0, PA5, (PA15)
	// Clock should be 32MHz

	// Enable output pin on PA0
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	GPIOA->MODER &= ~GPIO_MODER_MODE0_0;	// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)
	GPIOA->AFR[0] |= GPIO_AFRL_AFSEL0_0;	// Timer 2 Output channel is AF1

	// Timing calculations: Clock = 32MHz / (PSC + 1) = 4m counts per second
	// ARR = number of counts per PWM tick = 4096
	// 4m / ARR = 976.6Hz of PWM square wave with 4096 levels of output
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
	TIM2->CCMR1 |= TIM_CCMR1_OC1PE;			// Output compare 1 preload enable
	TIM2->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;		// 0110: PWM mode 1 - In upcounting, channel 1 is active as long as TIMx_CNT<TIMx_CCR1
	TIM2->CCR1 = 0x800;
	TIM2->ARR = 0xFFF;
	TIM2->PSC = 1;							// Should give ~4kHz
	TIM2->CR1 |= TIM_CR1_ARPE;				// 1: TIMx_ARR register is buffered
	TIM2->CCER |= TIM_CCER_CC1E;			// Capture mode enabled / OC1 signal is output on the corresponding output pin
	TIM2->EGR |= TIM_EGR_UG;				// 1: Re-initialize the counter and generates an update of the registers
	TIM2->CR1 |= TIM_CR1_CEN;				// Enable counter


	// Init debug pin PB8
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	GPIOB->MODER &= ~GPIO_MODER_MODE8_1;	// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)

}
