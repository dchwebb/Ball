#include "initialisation.h"
#include "stm32wb55xx.h"
#include "otp.h"

static void Reset_IPCC();
static void Reset_BackupDomain();
static void Config_HSE();


void SystemClock_Config()
{
	PWR->CR1 |= PWR_CR1_DBP; 						// Disable backup domain write protection: Enable access to the RTC registers
	PWR->CR1 |= 1 << PWR_CR1_VOS_Pos;				// Should be already set at Range 1: High-performance range (Range 2 is slower, low power)

	RCC->CR |= RCC_CR_HSEON;						// Turn on external oscillator
	while ((RCC->CR & RCC_CR_HSERDY) == 0);			// Wait till HSE is ready

	RCC->CR |= RCC_CR_HSION;						// Turn on high speed internal oscillator
	while ((RCC->CR & RCC_CR_HSIRDY) == 0);			// Wait till HSI is ready

	RCC->BDCR |= RCC_BDCR_LSEON;			// already on ???
	while ((RCC->BDCR & RCC_BDCR_LSERDY) == 0);		// Wait till HSI is ready

	MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, 1);	// Increase Flash latency as using clock greater than 18MHz (see manual p.77)
	while ((FLASH->ACR & FLASH_ACR_LATENCY) != 1);

	MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, 0b10);		// 10: HSE selected as system clock
	while ((RCC->CFGR & RCC_CFGR_SWS) != 0b10);		// Wait until HSE is selected

	SysTick->LOAD  = 32000 - 1;						// set reload register
	SysTick->VAL = 0;								// Load the SysTick Counter Value

	// Peripheral clocks already set to default: RTC, USART, LPUSART
	RCC->CSR |=  RCC_CSR_RFWKPSEL_0;				//  RF system wakeup clock source selection: 01: LSE oscillator clock
	RCC->SMPSCR |= RCC_SMPSCR_SMPSDIV_0;			// SMPS prescaler - FIXME not planning to use (see p214)
	RCC->SMPSCR &= ~ RCC_SMPSCR_SMPSSEL_Msk;		// 00: HSI16 selected as SMPS step-down converter clock

}




void InitTimer() {
	// TIM2 Channel 1 Output: *PA0, PA5, (PA15)
	// Clock should be 32MHz

	// Enable output pin on PA0
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	GPIOA->MODER &= ~GPIO_MODER_MODE0_0;		// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)
	GPIOA->AFR[0] |= GPIO_AFRL_AFSEL0_0;		// Timer 2 Output channel is AF1

	// Timing calculations: Clock = 32MHz / (PSC + 1) = 4m counts per second
	// ARR = number of counts per PWM tick = 4096
	// 4m / ARR = 976.6Hz of PWM square wave with 4096 levels of output
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
	TIM2->CCMR1 |= TIM_CCMR1_OC1PE;				// Output compare 1 preload enable
	TIM2->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;		// 0110: PWM mode 1 - In upcounting, channel 1 is active as long as TIMx_CNT<TIMx_CCR1
	TIM2->CCR1 = 0x800;
	TIM2->ARR = 0xFFF;
	TIM2->PSC = 1;								// Should give ~4kHz
	TIM2->CR1 |= TIM_CR1_ARPE;					// 1: TIMx_ARR register is buffered
	TIM2->CCER |= TIM_CCER_CC1E;				// Capture mode enabled / OC1 signal is output on the corresponding output pin
	TIM2->EGR |= TIM_EGR_UG;					// 1: Re-initialize the counter and generates an update of the registers
	TIM2->CR1 |= TIM_CR1_CEN;					// Enable counter


	// Init debug pin PB8
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	GPIOB->MODER &= ~GPIO_MODER_MODE8_1;		// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)

}


void InitHardware()
{
	FLASH->ACR |= FLASH_ACR_PRFTEN;

	NVIC_SetPriorityGrouping(0x3);				// Set NVIC Priority grouping to 4

	// Enable SysTick
	SysTick->LOAD  = 4000 - 1UL;				// set reload register
	SysTick->VAL = 0UL;							// Load the SysTick Counter Value
	NVIC_SetPriority(SysTick_IRQn, 0);
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
			SysTick_CTRL_TICKINT_Msk  |
			SysTick_CTRL_ENABLE_Msk;    		// Enable SysTick IRQ and SysTick Timer */
	extern uint32_t uwTickPrio;
	uwTickPrio = 0;

	// Enable hardware semaphore clock
	RCC->AHB3ENR |= RCC_AHB3ENR_HSEMEN;
	while ((RCC->AHB3ENR & RCC_AHB3ENR_HSEMEN) == 0) {}
	NVIC_SetPriority(HSEM_IRQn, 0);
	NVIC_EnableIRQ(HSEM_IRQn);

	FLASH->SR &= ~FLASH_SR_OPERR;				// Clear Flash Option Validity flag

	//Reset_BackupDomain();						// For restoring from battery power so not needed here
	Reset_IPCC();								// Enable IPCC clock and reset all channels
	Config_HSE();								// Set HSE capacitor tuning
}


static void Reset_BackupDomain()
{
	// Check if a reset has occurred
	if ((RCC->CSR & RCC_CSR_PINRSTF) && (RCC->CSR & RCC_CSR_SFTRSTF) == 0)	{
		PWR->CR1 |= PWR_CR1_DBP; 				// Disable backup domain write protection: Enable access to the RTC registers
		PWR->CR1 |= PWR_CR1_DBP;				// Write the value twice to flush the APB-AHB bridge

		RCC->BDCR |= RCC_BDCR_BDRST;			// Force Backup Domain Reset
		RCC->BDCR &= ~RCC_BDCR_BDRST;			// Release Backup Domain Reset
	}
}


static void Reset_IPCC()
{
	RCC->AHB3ENR |= RCC_AHB3ENR_IPCCEN;			// Enable IPCC Clock

	IPCC->C1SCR = 0x3F;							// Clear processor 1 all IPCC channels
	IPCC->C2SCR = 0x3F;							// Clear processor 2 all IPCC channels
	IPCC->C1MR |= 0x3F << IPCC_C1MR_CH1FM_Pos;	// Mask transmit channel free interrupt for processor 1
	IPCC->C2MR |= 0x3F << IPCC_C2MR_CH1FM_Pos;	// Mask transmit channel free interrupt for processor 2
	IPCC->C1MR |= 0x3F;							// Mask receive channel occupied interrupt for processor 1
	IPCC->C2MR |= 0x3F;							// Mask receive channel occupied interrupt for processor 2
}


static void Config_HSE(void)
{
	// Read HSE_Tuning from OTP and set HSE capacitor tuning
	OTP_ID0_t* p_otp = (OTP_ID0_t*) OTP_Read(0);
	if (p_otp) {
		RCC->HSECR = 0xCAFECAFE;		// HSE control unlock key
		MODIFY_REG(RCC->HSECR, RCC_HSECR_HSETUNE, p_otp->hse_tuning << RCC_HSECR_HSETUNE_Pos);
	}
}
