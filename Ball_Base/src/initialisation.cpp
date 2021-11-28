#include "initialisation.h"
#include "otp.h"

static void InitIPCC();
static void InitSysTick();
static void InitRTC();
static void InitGPIO();

#define IPCC_ALL_RX_BUF 0x0000003FU /*!< Mask for all RX buffers. */
#define IPCC_ALL_TX_BUF 0x003F0000U /*!< Mask for all TX buffers. */

void SystemClock_Config()
{
	FLASH->ACR |= FLASH_ACR_PRFTEN;					// Flash prefetch enable
	FLASH->SR &= ~FLASH_SR_OPERR;					// Clear Flash Option Validity flag

	// Read HSE_Tuning from OTP and set HSE capacitor tuning
	OTP_ID0_t* p_otp = (OTP_ID0_t*) OTP_Read(0);
	if (p_otp) {
		RCC->HSECR = 0xCAFECAFE;		// HSE control unlock key
		MODIFY_REG(RCC->HSECR, RCC_HSECR_HSETUNE, p_otp->hse_tuning << RCC_HSECR_HSETUNE_Pos);
	}

	PWR->CR1 |= PWR_CR1_DBP; 						// Disable backup domain write protection: Enable access to the RTC registers

	RCC->CR |= RCC_CR_HSEON;						// Turn on external oscillator
	while ((RCC->CR & RCC_CR_HSERDY) == 0);			// Wait till HSE is ready

	RCC->CR |= RCC_CR_HSION;						// Turn on high speed internal oscillator
	while ((RCC->CR & RCC_CR_HSIRDY) == 0);			// Wait till HSI is ready

	RCC->BDCR |= RCC_BDCR_LSEON;					// Turn on low speed external oscillator
	while ((RCC->BDCR & RCC_BDCR_LSERDY) == 0);		// Wait till LSE is ready

	MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, 1);	// Increase Flash latency as using clock greater than 18MHz (see manual p.77)
	while ((FLASH->ACR & FLASH_ACR_LATENCY) != 1);

	MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, 0b10);		// 10: HSE selected as system clock
	while ((RCC->CFGR & RCC_CFGR_SWS) == 0);		// Wait until HSE is selected

	SysTick->LOAD  = 32000 - 1;						// set reload register
	SysTick->VAL = 0;								// Load the SysTick Counter Value

	// Peripheral clocks already set to default: RTC, USART, LPUSART
	RCC->CSR |=  RCC_CSR_RFWKPSEL_0;				//  RF system wakeup clock source selection: 01: LSE oscillator clock
	RCC->SMPSCR |= RCC_SMPSCR_SMPSDIV_0;			// SMPS prescaler - FIXME not planning to use (see p214)
	RCC->SMPSCR &= ~RCC_SMPSCR_SMPSSEL_Msk;			// 00: HSI16 selected as SMPS step-down converter clock

}


void InitHardware()
{
	NVIC_SetPriorityGrouping(0x3);					// Set NVIC Priority grouping to 4
	InitSysTick();									// Initialise SysTick to 1kHz??

	// Enable hardware semaphore clock
	RCC->AHB3ENR |= RCC_AHB3ENR_HSEMEN;
	while ((RCC->AHB3ENR & RCC_AHB3ENR_HSEMEN) == 0);
	NVIC_SetPriority(HSEM_IRQn, 0);
	NVIC_EnableIRQ(HSEM_IRQn);

	InitIPCC();										// Enable IPCC clock and reset all channels
	InitRTC();										// Initialise RTC

	// Disable all wakeup interrupt on CPU1  except IPCC(36), HSEM(38)
	// see 369 / 1532 for Interrupt list table
	LL_EXTI_DisableIT_0_31(~0);
	LL_EXTI_DisableIT_32_63( (~0) & (~(LL_EXTI_LINE_36 | LL_EXTI_LINE_38)) );

	RCC->CFGR |= RCC_CFGR_STOPWUCK;					// 1: HSI16 oscillator selected as wakeup from stop clock and CSS backup clock

	// These bits select the low-power mode entered when CPU2 enters the deepsleep mode. The
	// system low-power mode entered depend also on the PWR_CR1.LPMS allowed low-power mode from CPU1.
	PWR->C2CR1 |= 4 & PWR_C2CR1_LPMS_Msk;			// 1xx: Shutdown mode

	InitGPIO();
}


static void InitGPIO()
{
	// GPIO Ports Clock Enable
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIODEN;

	// Configure GPIO pin : PC4 BUTTON_SW1_Pin FIXME GPIO C not available on WB35
	GPIOC->MODER &= ~GPIO_MODER_MODE4_Msk;			// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)
	GPIOC->PUPDR |= GPIO_PUPDR_PUPD4_0;				// Activate pull up
	SYSCFG->EXTICR[1] |= SYSCFG_EXTICR2_EXTI4_PC;	// Enable external interrupt
	EXTI->IMR1 |= EXTI_IMR1_IM4;					// 1: Wakeup with interrupt request from Line x is unmasked
	EXTI->FTSR1 |= EXTI_FTSR1_FT4;					// Enable falling edge trigger

	// Configure GPIO pins :  PD0 BUTTON_SW2_Pin; PD1 BUTTON_SW3_Pin FIXME GPIO D not available on WB35
	GPIOD->MODER &= ~(GPIO_MODER_MODE0_Msk | GPIO_MODER_MODE1_Msk);				// 00: Input mode
	GPIOD->PUPDR |= (GPIO_PUPDR_PUPD0_0 | GPIO_PUPDR_PUPD1_0);					// Activate pull up
	SYSCFG->EXTICR[0] |= (SYSCFG_EXTICR1_EXTI0_PD | SYSCFG_EXTICR1_EXTI1_PD);	// Enable external interrupt
	EXTI->IMR1 |= (EXTI_IMR1_IM0 | EXTI_IMR1_IM1);								// 1: Wakeup with interrupt request from Line x is unmasked
	EXTI->FTSR1 |= (EXTI_FTSR1_FT0 | EXTI_FTSR1_FT1);							// Enable falling edge trigger

	// Configure LED pins : PB0 LED_GREEN_Pin;  PB1 LED_RED_Pin; PB5 LED_BLUE_Pin
	GPIOB->MODER &= ~(GPIO_MODER_MODE0_1 | GPIO_MODER_MODE1_1 | GPIO_MODER_MODE5_1);

	// EXTI interrupt init
	NVIC_SetPriority(EXTI0_IRQn, 3);
	NVIC_EnableIRQ(EXTI0_IRQn);

	NVIC_SetPriority(EXTI1_IRQn, 3);
	NVIC_EnableIRQ(EXTI1_IRQn);

	NVIC_SetPriority(EXTI4_IRQn, 3);
	NVIC_EnableIRQ(EXTI4_IRQn);

	// Init debug pin PB8
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	GPIOB->MODER &= ~GPIO_MODER_MODE8_1;			// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)

}


void InitPWMTimer()
{
	// TIM2: Channel 1 Output: *PA0, PA5, (PA15); Channel 2: *PA1, PB3; Channel 3: PA2, PB10; Channel 4: PA3, PB11
	// Clock should be 32MHz

	// Enable channel 1, 2 PWM output pins on PA0, PA1
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	GPIOA->MODER &= ~(GPIO_MODER_MODE0_0 | GPIO_MODER_MODE1_0);			// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)
	GPIOA->AFR[0] |= (GPIO_AFRL_AFSEL0_0 | GPIO_AFRL_AFSEL1_0);			// Timer 2 Output channel is AF1

	// Timing calculations: Clock = 32MHz / (PSC + 1) = 4m counts per second
	// ARR = number of counts per PWM tick = 4096
	// 4m / ARR = 976.6Hz of PWM square wave with 4096 levels of output
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
	TIM2->CCMR1 |= (TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE);					// Output compare 1 and 2 preload enable
	TIM2->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;					// 0110: PWM mode 1 - In upcounting, channel 1 is active as long as TIMx_CNT<TIMx_CCR1
	TIM2->CCMR1 |= TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;					// 0110: PWM mode 1 - In upcounting, channel 1 is active as long as TIMx_CNT<TIMx_CCR1

	TIM2->CCR1 = 0x800;								// PWM level set in ble_hid.cpp
	TIM2->ARR = 0xFFF;								// Total number of PWM ticks = 4096
	TIM2->PSC = 1;									// Should give ~4kHz
	TIM2->CR1 |= TIM_CR1_ARPE;						// 1: TIMx_ARR register is buffered
	TIM2->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC2E);					// Capture mode enabled / OC1 signal is output on the corresponding output pin
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


static void InitSysTick()
{
	SysTick_Config(SystemCoreClock / SYSTICK);		// gives 1ms
	NVIC_SetPriority(SysTick_IRQn, 0);

	// Currently systick increments uwTick at 1 kHz
//	SysTick->LOAD  = 32000 - 1UL;					// set reload register
//	SysTick->VAL = 0UL;								// Load the SysTick Counter Value
//	NVIC_SetPriority(SysTick_IRQn, 0);
//	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
//			SysTick_CTRL_TICKINT_Msk  |
//			SysTick_CTRL_ENABLE_Msk;    			// Enable SysTick IRQ and SysTick Timer

}



