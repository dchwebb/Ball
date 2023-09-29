#include "initialisation.h"
#include "otp.h"

static void InitIPCC();
static void InitSysTick();
static void InitGPIO();


uint8_t hse_tuning = 19;		// Random guess based on Nucleo setting - doesn't seem to make much difference to current connection failure

void SystemClock_Config()
{
	FLASH->ACR |= FLASH_ACR_PRFTEN;					// Flash prefetch enable
	FLASH->SR &= ~FLASH_SR_OPERR;					// Clear Flash Option Validity flag

	// Read HSE_Tuning from OTP and set HSE capacitor tuning
//	OTP_ID0_t* p_otp = (OTP_ID0_t*) OTP_Read(0);
//	if (p_otp) {
//		RCC->HSECR = 0xCAFECAFE;					// HSE control unlock key
//		MODIFY_REG(RCC->HSECR, RCC_HSECR_HSETUNE, p_otp->hse_tuning << RCC_HSECR_HSETUNE_Pos);
//	}
	RCC->HSECR = 0xCAFECAFE;						// HSE control unlock key
	MODIFY_REG(RCC->HSECR, RCC_HSECR_HSETUNE, hse_tuning << RCC_HSECR_HSETUNE_Pos);


	PWR->CR1 |= PWR_CR1_DBP; 						// Disable backup domain write protection: Enable access to the RTC registers

	RCC->CR |= RCC_CR_HSEON;						// Turn on external oscillator
	while ((RCC->CR & RCC_CR_HSERDY) == 0);			// Wait till HSE is ready

	RCC->CR |= RCC_CR_HSION;						// Turn on high speed internal oscillator
	while ((RCC->CR & RCC_CR_HSIRDY) == 0);			// Wait till HSI is ready

	// Activate PLL: HSE = 32MHz / 4(M) * 32(N) / 4(R) = 64MHz
	RCC->PLLCFGR |= LL_RCC_PLLSOURCE_HSE;			// Set PLL source to HSE
	RCC->PLLCFGR |= LL_RCC_PLLM_DIV_4;				// Set PLL M divider to 4
	MODIFY_REG(RCC->PLLCFGR, RCC_PLLCFGR_PLLN, 32 << RCC_PLLCFGR_PLLN_Pos);		// Set PLL N multiplier to 32
	RCC->PLLCFGR |= LL_RCC_PLLR_DIV_4;				// Set PLL R divider to 4
	RCC->PLLCFGR |= RCC_PLLCFGR_PLLREN;				// Enable PLL R Clock

	RCC->CR |= RCC_CR_PLLON;						// Turn on PLL
	while ((RCC->CR & RCC_CR_PLLRDY) == 0);			// Wait till PLL is ready

	MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, 3);	// Increase Flash latency to 3 Wait States (see manual p.77)
	while ((FLASH->ACR & FLASH_ACR_LATENCY) != 3);

	RCC->CFGR |= RCC_CFGR_SW;						// 11: PLL selected as system clock
	while ((RCC->CFGR & RCC_CFGR_SWS) == 0);		// Wait until PLL is selected

	RCC->EXTCFGR |= RCC_EXTCFGR_C2HPRE_3;			// 1000: CPU2 HPrescaler: SYSCLK divided by 2

	RCC->CSR |= RCC_CSR_RFWKPSEL;					// RF system wakeup clock source selection: 11: HSE oscillator clock divided by 1024 used as RF system wakeup clock
}


void InitHardware()
{
	NVIC_SetPriorityGrouping(0x3);					// Set NVIC Priority grouping to 4
	InitSysTick();									// Initialise SysTick to 1kHz??

	// Enable hardware semaphore clock
	RCC->AHB3ENR |= RCC_AHB3ENR_HSEMEN;
	while ((RCC->AHB3ENR & RCC_AHB3ENR_HSEMEN) == 0);
//	NVIC_SetPriority(HSEM_IRQn, 0);
//	NVIC_EnableIRQ(HSEM_IRQn);

	InitIPCC();										// Enable IPCC clock and reset all channels

	// Disable all wakeup interrupt on CPU1  except IPCC(36), HSEM(38)
	// see 369 / 1532 for Interrupt list table
	LL_EXTI_DisableIT_0_31(~0);
	LL_EXTI_DisableIT_32_63( (~0) & (~(LL_EXTI_LINE_36 | LL_EXTI_LINE_38)) );

	InitGPIO();
}


static void InitGPIO()
{
	// GPIO Ports Clock Enable
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

	// Configure GPIO pin : PA4 Connect Button (BUTTON_SW1_Pin)
	GPIOA->MODER &= ~GPIO_MODER_MODE4_Msk;			// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)
	GPIOA->PUPDR |= GPIO_PUPDR_PUPD4_0;				// Activate pull up
	SYSCFG->EXTICR[1] |= SYSCFG_EXTICR2_EXTI4_PA;	// Enable external interrupt
	EXTI->IMR1 |= EXTI_IMR1_IM4;					// 1: Wakeup with interrupt request from Line x is unmasked
	EXTI->FTSR1 |= EXTI_FTSR1_FT4;					// Enable falling edge trigger

	NVIC_SetPriority(EXTI4_IRQn, 3);				// EXTI interrupt init
	NVIC_EnableIRQ(EXTI4_IRQn);


	GPIOA->MODER &= ~GPIO_MODER_MODE3_1;			// Configure Connection LED pin: PA3

	// Init debug pin PB8
//	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
//	GPIOB->MODER &= ~GPIO_MODER_MODE8_1;			// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)

}


void InitPWMTimer()
{
	// TIM2: Channel 1 Output: *PA0, PA5, (PA15); Channel 2: *PA1, PB3; Channel 3: *PA2, PB10; Channel 4: PA3, PB11
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

	// Enable channel 1, 2, 3 PWM output pins on PA0, PA1, PA2
	GPIOA->MODER &= ~(GPIO_MODER_MODE0_0 | GPIO_MODER_MODE1_0 | GPIO_MODER_MODE2_0);			// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)
	GPIOA->AFR[0] |= (GPIO_AFRL_AFSEL0_0 | GPIO_AFRL_AFSEL1_0 | GPIO_AFRL_AFSEL2_0);			// Timer 2 Output channel is AF1

	// Timing calculations: Clock = 64MHz / (PSC + 1) = 32m counts per second
	// ARR = number of counts per PWM tick = 4096
	// 32m / ARR = 7.812kHz of PWM square wave with 4096 levels of output
	TIM2->CCMR1 |= TIM_CCMR1_OC1PE;					// Output compare 1 preload enable
	TIM2->CCMR1 |= TIM_CCMR1_OC2PE;					// Output compare 2 preload enable
	TIM2->CCMR2 |= TIM_CCMR2_OC3PE;					// Output compare 3 preload enable

	TIM2->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2);	// 0110: PWM mode 1 - In upcounting, channel 1 active if TIMx_CNT<TIMx_CCR1
	TIM2->CCMR1 |= (TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2);	// 0110: PWM mode 1 - In upcounting, channel 2 active if TIMx_CNT<TIMx_CCR2
	TIM2->CCMR2 |= (TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2);	// 0110: PWM mode 1 - In upcounting, channel 3 active if TIMx_CNT<TIMx_CCR3

	TIM2->CCR1 = 0x800;								// Initialise PWM level to midpoint (PWM level set in ble_hid.cpp)
	TIM2->CCR2 = 0x800;
	TIM2->CCR3 = 0x800;

	TIM2->ARR = 0xFFF;								// Total number of PWM ticks = 4096
	TIM2->PSC = 1;									// Should give ~7.8kHz
	TIM2->CR1 |= TIM_CR1_ARPE;						// 1: TIMx_ARR register is buffered
	TIM2->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E);		// Capture mode enabled / OC1 signal is output on the corresponding output pin
	TIM2->EGR |= TIM_EGR_UG;						// 1: Re-initialize the counter and generates an update of the registers
	TIM2->CR1 |= TIM_CR1_CEN;						// Enable counter
}


static void InitIPCC()
{
	RCC->AHB3ENR |= RCC_AHB3ENR_IPCCEN;				// Enable IPCC Clock

	IPCC->C1SCR = 0x3F;								// Clear processor 1 all IPCC channels
	IPCC->C2SCR = 0x3F;								// Clear processor 2 all IPCC channels
	IPCC->C1MR |= 0x3F << IPCC_C1MR_CH1FM_Pos;		// Mask transmit channel free interrupt for processor 1
	IPCC->C2MR |= 0x3F << IPCC_C2MR_CH1FM_Pos;		// Mask transmit channel free interrupt for processor 2
	IPCC->C1MR |= 0x3F;								// Mask receive channel occupied interrupt for processor 1
	IPCC->C2MR |= 0x3F;								// Mask receive channel occupied interrupt for processor 2

	NVIC_SetPriority(IPCC_C1_RX_IRQn, 0);
	NVIC_EnableIRQ(IPCC_C1_RX_IRQn);
	NVIC_SetPriority(IPCC_C1_TX_IRQn, 0);
	NVIC_EnableIRQ(IPCC_C1_TX_IRQn);

	IPCC->C1CR |= (IPCC_CR_RXOIE | IPCC_CR_TXFIE);	// Activate the interrupts
}


#ifdef USERTC
static void InitRTC()
{
	RCC->BDCR |= RCC_BDCR_RTCEN;					// Enable RTC
	RCC->BDCR |= RCC_BDCR_RTCSEL_0;					// Set RTC CLock to source to LSE
	RCC->APB1ENR1 |= RCC_APB1ENR1_RTCAPBEN;			// CPU1 RTC APB clock enable
	while ((RCC->APB1ENR1 & RCC_APB1ENR1_RTCAPBEN) == 0);

	RTC->WPR = 0xCAU;								// Disable the write protection for RTC registers - see p.919
	RTC->WPR = 0x53U;

	RTC->ISR = 0xFFFFFFFF;							// Enter the Initialization mode (Just setting the Init Flag does not seem to work)
	while ((RTC->ISR & RTC_ISR_INITF) == 0);

	RTC->CR |= RTC_CR_WUTE;							// Enable Wake up timer see p918
	RTC->ISR &= ~RTC_ISR_INIT;						// Exit Initialization mode

	RTC->WPR = 0xFFU;								// Enable the write protection for RTC registers.
}
#endif

static void InitSysTick()
{
	SysTick_Config(SystemCoreClock / SYSTICK);		// gives 1ms
	NVIC_SetPriority(SysTick_IRQn, 0);
}



