#include "initialisation.h"
#include "otp.h"

static void InitIPCC();
static void InitSysTick();
static void InitRTC();
static void InitGPIO();
static void InitADC();
static void InitSPI();


uint8_t hse_tuning = 19;		// Random guess based on Nucleo setting - doesn't seem to make much difference

void InitClocks()
{
	FLASH->ACR |= FLASH_ACR_PRFTEN;					// Flash prefetch enable
	FLASH->SR &= ~FLASH_SR_OPERR;					// Clear Flash Option Validity flag

/*
	// Read HSE_Tuning from OTP and set HSE capacitor tuning
	OTP_ID0_t* p_otp = (OTP_ID0_t*) OTP_Read(0);
	if (p_otp) {
		RCC->HSECR = 0xCAFECAFE;					// HSE control unlock key
		MODIFY_REG(RCC->HSECR, RCC_HSECR_HSETUNE, p_otp->hse_tuning << RCC_HSECR_HSETUNE_Pos);
	}
	RCC->HSECR = 0xCAFECAFE;						// HSE control unlock key
	MODIFY_REG(RCC->HSECR, RCC_HSECR_HSETUNE, hse_tuning << RCC_HSECR_HSETUNE_Pos);
*/

	PWR->CR1 |= PWR_CR1_DBP; 						// Disable backup domain write protection: Enable access to the RTC registers

	RCC->CR |= RCC_CR_HSEON;						// Turn on external oscillator
	while ((RCC->CR & RCC_CR_HSERDY) == 0);			// Wait till HSE is ready

	RCC->CR |= RCC_CR_HSION;						// Turn on high speed internal oscillator
	while ((RCC->CR & RCC_CR_HSIRDY) == 0);			// Wait till HSI is ready

	RCC->BDCR |= RCC_BDCR_LSEON;					// Turn on low speed external oscillator
	while ((RCC->BDCR & RCC_BDCR_LSERDY) == 0);		// Wait till LSE is ready

	// Activate PLL: HSE = 32MHz / 4(M) * 32(N) / 4(R) = 64MHz
	RCC->PLLCFGR |= LL_RCC_PLLSOURCE_HSE;			// Set PLL source to HSE
	RCC->PLLCFGR |= LL_RCC_PLLM_DIV_4;				// Set PLL M divider to 4
	MODIFY_REG(RCC->PLLCFGR, RCC_PLLCFGR_PLLN, 32 << RCC_PLLCFGR_PLLN_Pos);		// Set PLL N multiplier to 32
	RCC->PLLCFGR |= LL_RCC_PLLR_DIV_4;				// Set PLL R divider to 4
	RCC->PLLCFGR |= RCC_PLLCFGR_PLLREN;				// Enable PLL R Clock

	RCC->CR |= RCC_CR_PLLON;						// Turn on PLL
	while ((RCC->CR & RCC_CR_PLLRDY) == 0);			// Wait till PLL is ready

	// Increase Flash latency to 3 Wait States (see manual p.77)
	MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_3WS);
	while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_3WS);

	RCC->CFGR |= RCC_CFGR_SW;						// 11: PLL selected as system clock
	while ((RCC->CFGR & RCC_CFGR_SWS) == 0);		// Wait until PLL is selected

	RCC->EXTCFGR |= RCC_EXTCFGR_C2HPRE_3;			// 1000: CPU2 HPrescaler: SYSCLK divided by 2

	RCC->CSR |= RCC_CSR_RFWKPSEL;					// RF system wakeup clock source selection: 11: HSE oscillator clock divided by 1024 used as RF system wakeup clock
	SystemCoreClockUpdate();						// Read configured clock speed into SystemCoreClock (system clock frequency)
}


// Used when sleeping to switch from HSE to HSI clock
void SwitchToHSI()
{
	MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, LL_RCC_SYS_CLKSOURCE_HSI);			// Set HSI as clock source
	while ((RCC->CFGR & RCC_CFGR_SWS) != LL_RCC_SYS_CLKSOURCE_STATUS_HSI);	// Wait until HSI is selected
}


void InitHardware()
{
	NVIC_SetPriorityGrouping(0x3);					// Set NVIC Priority grouping to 4
	InitSysTick();									// Initialise SysTick to 1kHz??

	// Enable hardware semaphore clock
	RCC->AHB3ENR |= RCC_AHB3ENR_HSEMEN;
	while ((RCC->AHB3ENR & RCC_AHB3ENR_HSEMEN) == 0);

	// Disable all wakeup interrupt on CPU1  except IPCC(36), HSEM(38)
	// see 369 / 1532 for Interrupt list table
	LL_EXTI_DisableIT_0_31(~0);
	LL_EXTI_DisableIT_32_63( (~0) & (~(LL_EXTI_LINE_36 | LL_EXTI_LINE_38)) );

	InitIPCC();										// Enable IPCC clock and reset all channels
	InitRTC();										// Initialise RTC
	InitGPIO();
	InitADC();
	InitSPI();
}


static void InitSPI()
{
	// Configure SPI 1 to communicate with gyroscope:  PB4 MISO; PB5 MOSI; PB3 CLK; PA15 CS (all Alternate Function 5)
	RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;				// Enable SPI Clock
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;			// GPIO Ports Clock Enable
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

	GPIOB->MODER &= ~(GPIO_MODER_MODE3_0 | GPIO_MODER_MODE4_0 | GPIO_MODER_MODE5_0);
	GPIOB->AFR[0] |= (5 << GPIO_AFRL_AFSEL3_Pos);	// CLK
	GPIOB->AFR[0] |= (5 << GPIO_AFRL_AFSEL4_Pos);	// MISO
	GPIOB->AFR[0] |= (5 << GPIO_AFRL_AFSEL5_Pos);	// MOSI
	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD4;				// PB4 is pulled up by default (also used as JTAG reset)

	GPIOA->MODER &= ~GPIO_MODER_MODE15;				// CS to output mode
	GPIOA->MODER |= GPIO_MODER_MODE15_0;
	GPIOA->ODR |= GPIO_ODR_OD15;					// Set high
	GPIOA->PUPDR &= ~GPIO_PUPDR_PUPD15;				// PA15 is pulled up by default (also used as JTDI)
//	GPIOA->MODER &= ~GPIO_MODER_MODE15_0;
//	GPIOA->AFR[1] |= (5 << GPIO_AFRH_AFSEL15_Pos);	// CS AF 5
//	GPIOA->PUPDR |= GPIO_PUPDR_PUPD15_0;


	SPI1->CR1 |= SPI_CR1_MSTR;						// Master mode
	SPI1->CR1 |= SPI_CR1_SSI;						// Internal slave select
	SPI1->CR1 |= SPI_CR1_SSM;						// Software CS management
//	SPI1->CR2 |= SPI_CR2_SSOE;						// NSS (CS) output enable
//	SPI1->CR2 |= SPI_CR2_NSSP;						// Pulse Chip select line on communication

	SPI1->CR1 |= 0b011 << SPI_CR1_BR_Pos;			// 011: fPCLK/16 = 64Mhz/16 = 4MB/s APB2 clock currently 64MHz (FIXME this should be slower in final version)
	SPI1->CR1 |= SPI_CR1_SPE;
}


static void InitADC()
{
	// For measuring battery level on PA1 ADC1_IN6
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;			// GPIO Ports Clock Enable
	GPIOA->MODER |= GPIO_MODER_MODE1;				// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)

	RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;				// Enable ADC
	RCC->CCIPR |= RCC_CCIPR_ADCSEL;					// 00: None; 01: PLLSAI1 R clock; 10: PLL P clock; 11: System clock

	ADC1_COMMON->CCR |= ADC_CCR_PRESC_1;			// 0010: input ADC clock divided by 4

	ADC1->CR &= ~ADC_CR_DEEPPWD;					// Deep power down: 0: ADC not in deep-power down	1: ADC in deep-power-down (default reset state)
	ADC1->CR |= ADC_CR_ADVREGEN;					// Enable ADC internal voltage regulator
	while ((ADC1->CR & ADC_CR_ADVREGEN) != ADC_CR_ADVREGEN) {}

	ADC1->SQR1 |= 6 << ADC_SQR1_SQ1_Pos;
	ADC1->SMPR1 |= 0b010 << ADC_SMPR1_SMP6_Pos;

	// Start calibration
	ADC1->CR &= ~ADC_CR_ADCALDIF;					// Calibration in single ended mode
	ADC1->CR |= ADC_CR_ADCAL;
	while ((ADC1->CR & ADC_CR_ADCAL) == ADC_CR_ADCAL) {};

	ADC1->CR |= ADC_CR_ADEN;						// Enable ADC
	while ((ADC1->ISR & ADC_ISR_ADRDY) == 0) {}

	ADC1->CR |= ADC_CR_ADSTART;						// Trigger next ADC read

}


static void InitGPIO()
{
	// GPIO Ports Clock Enable
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

	// Configure GPIO pin : PA4 Connect Button
	GPIOA->MODER &= ~GPIO_MODER_MODE4_Msk;			// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)
	GPIOA->PUPDR |= GPIO_PUPDR_PUPD4_0;				// Activate pull up

	// Enable EXTI WKUP4 on PA4 (marked as 'Connect') to wake up from shutdown
	SYSCFG->EXTICR[1] |= SYSCFG_EXTICR2_EXTI4_PA;	// Enable external interrupt
	EXTI->IMR1 |= EXTI_IMR1_IM4;					// 1: CPU1 Wakeup with interrupt request from Line x is unmasked
	EXTI->FTSR1 |= EXTI_FTSR1_FT4;					// Enable falling edge trigger
	PWR->CR4 &= ~PWR_CR4_WP4;						// Wake-Up pin polarity (0 = rising 1 = falling edge)
	PWR->CR3 |= PWR_CR3_EWUP4;						// Enable WKUP4 on PA4

	NVIC_SetPriority(EXTI4_IRQn, 3);				// EXTI interrupt init
	NVIC_EnableIRQ(EXTI4_IRQn);

	// Enable EXTI WKUP9 on PB9 (marked as 'I2C SDA') to wake up from shutdown on gyro interrupt
	GPIOB->MODER &= ~GPIO_MODER_MODE9_Msk;			// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)
	SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI9_PB;	// Enable external interrupt
	EXTI->IMR1 |= EXTI_IMR1_IM9;					// 1: CPU1 Wakeup with interrupt request from Line x is unmasked
	EXTI->RTSR1 |= EXTI_RTSR1_RT9;					// Enable rising edge trigger

	// Enable EXTI WKUP8 on PB8 (marked as 'I2C CLK') to interrupt on gyro ready pin
	GPIOB->MODER &= ~GPIO_MODER_MODE8_Msk;			// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)
	SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI8_PB;	// Enable external interrupt
	EXTI->IMR1 |= EXTI_IMR1_IM8;					// 1: CPU1 Wakeup with interrupt request from Line x is unmasked
	EXTI->RTSR1 |= EXTI_RTSR1_RT8;					// Enable rising edge trigger

	NVIC_SetPriority(EXTI9_5_IRQn, 3);				// EXTI interrupt init
	NVIC_EnableIRQ(EXTI9_5_IRQn);


	GPIOA->MODER &= ~GPIO_MODER_MODE3_1;			// Configure LED pins : PA3 Connect LED

//	GPIOB->MODER &= ~GPIO_MODER_MODE8_1;			// Configure pin PB8 (marked as I2C CLK) for debug output

	// NB p159 Footnote 11: The I/Os with wake-up from Standby/Shutdown capability are PA0 and PA2
}


static void InitIPCC()
{
	// Inter-processor communication controller (IPCC) is used for communicating data between two processors
	RCC->AHB3ENR |= RCC_AHB3ENR_IPCCEN;				// Enable IPCC Clock

	IPCC->C1SCR = 0x3F;								// Clear processor 1 all IPCC channels
	IPCC->C2SCR = 0x3F;								// Clear processor 2 all IPCC channels
	IPCC->C1MR |= 0x3F << IPCC_C1MR_CH1FM_Pos;		// Mask transmit channel free interrupt for processor 1
	IPCC->C2MR |= 0x3F << IPCC_C2MR_CH1FM_Pos;		// Mask transmit channel free interrupt for processor 2
	IPCC->C1MR |= 0x3F;								// Mask receive channel occupied interrupt for processor 1
	IPCC->C2MR |= 0x3F;								// Mask receive channel occupied interrupt for processor 2

	NVIC_SetPriority(IPCC_C1_RX_IRQn, 1);
	NVIC_EnableIRQ(IPCC_C1_RX_IRQn);
	NVIC_SetPriority(IPCC_C1_TX_IRQn, 1);
	NVIC_EnableIRQ(IPCC_C1_TX_IRQn);

	IPCC->C1CR |= (IPCC_CR_RXOIE | IPCC_CR_TXFIE);	// Activate the interrupts
}


static void InitRTC()
{
	// NB RTC->PRER prescaler is already configured to give a 1Hz clock from the 32.768kHz external crystal
	RCC->BDCR |= RCC_BDCR_RTCEN;					// Enable RTC
	RCC->BDCR |= RCC_BDCR_RTCSEL_0;					// Set RTC Clock to source to LSE

	RTC->WPR = 0xCAU;								// Disable the write protection for RTC registers - see p.919
	RTC->WPR = 0x53U;

	RTC->ISR = 0xFFFFFFFF;							// Enter the Initialization mode (Just setting the Init Flag does not seem to work)
	while ((RTC->ISR & RTC_ISR_INITF) == 0);

	RCC->APB1ENR1 |= RCC_APB1ENR1_RTCAPBEN;			// CPU1 RTC APB clock enable
	while ((RCC->APB1ENR1 & RCC_APB1ENR1_RTCAPBEN) == 0);

	RTC->CR |= RTC_CR_WUTE;							// Enable Wake up timer see p918
	RTC->CR |= RTC_CR_WUTIE;						// Enable interrupt in RTC module
	RTC->ISR &= ~RTC_ISR_INIT;						// Exit Initialization mode

	RTC->WPR = 0xFFU;								// Enable the write protection for RTC registers.

	NVIC_SetPriority(RTC_WKUP_IRQn, 2);
	NVIC_EnableIRQ(RTC_WKUP_IRQn);

	EXTI->IMR1 |= EXTI_IMR1_IM19;					// Enable wake up timer interrupt on EXTI RTC_WKUP
	EXTI->RTSR1 |= EXTI_RTSR1_RT19;					// Trigger EXTI on rising edge
}


void RTCInterrupt(uint32_t seconds)
{
	// Pass seconds = 0 to disable

	RTC->WPR = 0xCAU;								// Disable the write protection for RTC registers - see p.919
	RTC->WPR = 0x53U;

	RTC->ISR |= RTC_ISR_INIT;						// Enter the Initialization mode (Just setting the Init Flag does not seem to work)
	while ((RTC->ISR & RTC_ISR_INITF) == 0);

	RTC->CR &= ~RTC_CR_WUTE;						// Clear Wake up timer
	while ((RTC->ISR & RTC_ISR_WUTWF) == 0);

	RTC->ISR &= ~RTC_ISR_WUTF;						// Clear Wake up timer flag

	if (seconds) {
		RTC->CR |= RTC_CR_WUCKSEL_2;				// 10x: ck_spre (usually 1 Hz) clock is selected
		RTC->WUTR = seconds;						// Set countdown time
		RTC->CR |= RTC_CR_WUTE;						// Enable Wake up timer
	} else {
		RTC->CR &= ~RTC_CR_WUTE;					// Disable Wake up timer// Diable
	}
	EXTI->PR1 = EXTI_PR1_PIF19;						// Clear pending register
	NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);

	RTC->ISR &= ~RTC_ISR_INIT;						// Exit Initialization mode
	RTC->WPR = 0xFFU;								// Enable the write protection for RTC registers.
}


static void InitSysTick()
{
	SysTick_Config(SystemCoreClock / SYSTICK);		// gives 1ms
	NVIC_SetPriority(SysTick_IRQn, 0);
}


void Init_Smps()
{
	//  To use the SMPS an inductor and capacitor need to be fitted (not on current hardware revision) Datasheet p27
	LL_PWR_SMPS_SetStartupCurrent(LL_PWR_SMPS_STARTUP_CURRENT_80MA);
	LL_PWR_SMPS_SetOutputVoltageLevel(LL_PWR_SMPS_OUTPUT_VOLTAGE_1V40);
	LL_PWR_SMPS_Enable();
}
