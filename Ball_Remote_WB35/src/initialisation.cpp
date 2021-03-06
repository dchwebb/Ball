#include "initialisation.h"
#include "otp.h"

static void InitIPCC();
static void InitSysTick();
static void InitRTC();
static void InitGPIO();
static void InitADC();
static void InitSPI();

#define IPCC_ALL_RX_BUF 0x0000003FU /*!< Mask for all RX buffers. */
#define IPCC_ALL_TX_BUF 0x003F0000U /*!< Mask for all TX buffers. */


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

	MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, 3);	// Increase Flash latency to 3 Wait States (see manual p.77)
	while ((FLASH->ACR & FLASH_ACR_LATENCY) != 3);

//	MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, 0b10);		// 10: HSE selected as system clock
//	while ((RCC->CFGR & RCC_CFGR_SWS) == 0);		// Wait until HSE is selected

	MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, LL_RCC_SYS_CLKSOURCE_PLL);		// 11: PLL selected as system clock
	while ((RCC->CFGR & RCC_CFGR_SWS) == 0);		// Wait until PLL is selected

	RCC->EXTCFGR |= RCC_EXTCFGR_C2HPRE_3;			// 1000: CPU2 HPrescaler: SYSCLK divided by 2

	// Peripheral clocks already set to default: RTC, USART, LPUSART
	//RCC->CSR |=  RCC_CSR_RFWKPSEL_0;				// RF system wakeup clock source selection: 01: LSE oscillator clock
	RCC->CSR |=  RCC_CSR_RFWKPSEL_0 | RCC_CSR_RFWKPSEL_1;				// RF system wakeup clock source selection: 11: HSE oscillator clock divided by 1024 used as RF system wakeup clock

}
/*
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

	RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_0;			// PLL source is MSI

	RCC->CR |= RCC_CR_HSEON;						// Turn on external oscillator
	while ((RCC->CR & RCC_CR_HSERDY) == 0);			// Wait till HSE is ready

	RCC->CR |= RCC_CR_HSION;						// Turn on high speed internal oscillator
	while ((RCC->CR & RCC_CR_HSIRDY) == 0);			// Wait till HSI is ready

#ifdef USEBASEBOARD
	RCC->CSR |= RCC_CSR_LSI1ON;
	while ((RCC->CSR & RCC_CSR_LSI1RDY) == 0);		// Wait till LSI is ready
#elif
	RCC->BDCR |= RCC_BDCR_LSEON;					// Turn on low speed external oscillator
	while ((RCC->BDCR & RCC_BDCR_LSERDY) == 0);		// Wait till LSE is ready
#endif

	MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, 1);	// Increase Flash latency as using clock greater than 18MHz (see manual p.77)
	while ((FLASH->ACR & FLASH_ACR_LATENCY) != 1);

	MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, 0b10);		// 10: HSE selected as system clock
	while ((RCC->CFGR & RCC_CFGR_SWS) == 0);		// Wait until HSE is selected

	SysTick->LOAD  = 32000 - 1;						// set reload register
	SysTick->VAL = 0;								// Load the SysTick Counter Value

	// Peripheral clocks already set to default: RTC, USART, LPUSART
	RCC->CSR |=  RCC_CSR_RFWKPSEL_0;				// RF system wakeup clock source selection: 01: LSE oscillator clock
	RCC->SMPSCR |= RCC_SMPSCR_SMPSDIV_0;			// SMPS prescaler - FIXME not planning to use (see p214)
	RCC->SMPSCR &= ~RCC_SMPSCR_SMPSSEL_Msk;			// 00: HSI16 selected as SMPS step-down converter clock

	RCC->CR |= RCC_CR_MSIPLLEN;						// Multispeed internal RC oscillator - used for USB?
}
*/


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


	InitGPIO();
	InitADC();
	InitSPI();

#ifndef USEBASEBOARD
//	InitI2C();
#endif
}


static void InitSPI()
{
	// Configure SPI 1 to communicate with gyroscope:  PB4 MISO; PB5 MOSI; PB3 CLK; PA15 CS (all Alternate Function 5)
	RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;				// Enable SPI Clock
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;			// GPIO Ports Clock Enable
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

	GPIOA->MODER &= ~GPIO_MODER_MODE15_0;			// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)
	GPIOB->MODER &= ~(GPIO_MODER_MODE3_0 | GPIO_MODER_MODE4_0 | GPIO_MODER_MODE5_0);
	GPIOA->AFR[1] |= (5 << GPIO_AFRH_AFSEL15_Pos);	// AF 5
	GPIOB->AFR[0] |= (5 << GPIO_AFRL_AFSEL3_Pos);
	GPIOB->AFR[0] |= (5 << GPIO_AFRL_AFSEL4_Pos);
	GPIOB->AFR[0] |= (5 << GPIO_AFRL_AFSEL5_Pos);
	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD4;				// PB4 is pulled up by default (also used as JTAG reset)

	SPI1->CR1 |= SPI_CR1_MSTR;						// Master mode
	SPI1->CR2 |= SPI_CR2_DS;						// Set data size to 16 bit
	SPI1->CR2 |= SPI_CR2_SSOE;						// NSS (CS) output enable
	SPI1->CR2 |= SPI_CR2_NSSP;						// Pulse Chip select line on communication

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


	GPIOA->MODER &= ~GPIO_MODER_MODE3_1;			// Configure LED pins : PA3 LED_BLUE_Pin


	/*
	// Init debug pin PB8
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	GPIOB->MODER &= ~GPIO_MODER_MODE8_1;			// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)
*/
	// Enable EXTI WKUP4 on PA2 to wake up from shutdown
	//EXTI->EMR1 |= EXTI_EMR1_EM4;
	PWR->CR4 &= ~PWR_CR4_WP4;		// Wake-Up pin polarity (0=rising 1 = falling edge)
	PWR->CR3 |= PWR_CR3_EWUP4;		// Enable WKUP4 on PA2
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
	RCC->BDCR |= RCC_BDCR_RTCSEL_0;					// Set RTC Clock to source to LSE

	RTC->WPR = 0xCAU;								// Disable the write protection for RTC registers - see p.919
	RTC->WPR = 0x53U;

	RTC->ISR = 0xFFFFFFFF;							// Enter the Initialization mode (Just setting the Init Flag does not seem to work)
	while ((RTC->ISR & RTC_ISR_INITF) == 0);

#ifdef USEBASEBOARD
	RCC->BDCR |= RCC_BDCR_RTCSEL_1;					// Set RTC Clock to source to LSI
	RTC->PRER = (255 << RTC_PRER_PREDIV_S_Pos) | (124 << RTC_PRER_PREDIV_A_Pos);		// Set prescaler for 32kHz input clock (1Hz = 32k / (124[PREDIV_A] + 1) * (255[PREDIV_S] + 1)
#else
	RCC->BDCR |= RCC_BDCR_RTCSEL_0;					// Set RTC Clock to source to LSE
#endif
	RCC->APB1ENR1 |= RCC_APB1ENR1_RTCAPBEN;			// CPU1 RTC APB clock enable
	while ((RCC->APB1ENR1 & RCC_APB1ENR1_RTCAPBEN) == 0);

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


void InitI2C()
{
	//	Enable GPIO and I2C clocks
	RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;			// I2C1 Peripheral Clocks Enable
	RCC->CCIPR &= ~RCC_CCIPR_I2C1SEL;				// 00: PCLK ; 01: System clock; 10: HSI16 clock
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;			// GPIO port clock
	RCC->AHB1ENR |= RCC_AHB1ENR_DMAMUX1EN;			// DMA Mux Clock
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;				// DMA 1 Clock

	// Initialize I2C DMA peripheral
	DMA1_Channel1->CCR &= ~DMA_CCR_EN;
	DMA1_Channel1->CCR &= ~DMA_CCR_CIRC;			// Disable Circular mode to keep refilling buffer
	DMA1_Channel1->CCR |= DMA_CCR_MINC;				// Memory in increment mode
	DMA1_Channel1->CCR &= ~DMA_CCR_PSIZE_0;			// Peripheral size: 00 = 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Channel1->CCR &= ~DMA_CCR_MSIZE_0;			// Memory size: 00 = 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Channel1->CCR |= DMA_CCR_PL_0;				// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High
	DMA1_Channel1->CCR &= ~DMA_CCR_DIR;				// data transfer direction: 00: peripheral-to-memory; 01: memory-to-peripheral; 10: memory-to-memory				// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High

	DMA1->IFCR |= 0xF << DMA_IFCR_CGIF1_Pos;		// clear all five interrupts for this stream

	DMAMUX1_Channel0->CCR |= 10; 					// DMA request MUX input 10 = I2C1_RX (See p.348)
	DMAMUX1_ChannelStatus->CFR |= DMAMUX_CFR_CSOF0; // Channel 0 Clear synchronization overrun event flag

	// PB8: I2C1_SCL [alternate function AF4]
	GPIOB->OTYPER |= GPIO_OTYPER_OT8;				// Set pin output to Open Drain
	GPIOB->MODER &= ~GPIO_MODER_MODE8_0;			// 10: Alternate function mode
	GPIOB->AFR[1] |= 4 << GPIO_AFRH_AFSEL8_Pos;		// Alternate Function 4 (I2C1)

	// PB9: I2C1_SDA [alternate function AF4]
	GPIOB->OTYPER |= GPIO_OTYPER_OT9;				// Set pin output to Open Drain
	GPIOB->MODER &= ~GPIO_MODER_MODE9_0;			// 10: Alternate function mode
	GPIOB->AFR[1] |= 4 << GPIO_AFRH_AFSEL9_Pos;		// Alternate Function 4 (I2C1)

	// Timings taken from HAL for 100kHz:
	// Timing calculations: I2C frequency: 1 / ((SCLL + 1) + (SCLH + 1)) * (PRESC + 1) * 1/I2CCLK)
	// [eg 1 / ((256 + 237) * 8 * 1/32MHz) = 100kHz] (Will actually be slightly slower as there are also sync times to be accounted for)
	I2C1->TIMINGR |= 0x7 << I2C_TIMINGR_PRESC_Pos;	// Timing prescaler
	I2C1->TIMINGR |= 0x2 << I2C_TIMINGR_SDADEL_Pos;	// Data Hold Time
	I2C1->TIMINGR |= 0x4 << I2C_TIMINGR_SCLDEL_Pos;	// Data Setup Time
	I2C1->TIMINGR |= 0x13 << I2C_TIMINGR_SCLL_Pos;	// SCLL low period
	I2C1->TIMINGR |= 0x0F << I2C_TIMINGR_SCLH_Pos;	// SCLH high period

	I2C1->CR1 &= ~I2C_CR1_NOSTRETCH;				// Clock stretching disable: Must be cleared in master mode

	NVIC_SetPriority(I2C1_EV_IRQn, 4);				// Enable I2C interrupts: Lower is higher priority
	NVIC_EnableIRQ(I2C1_EV_IRQn);

	I2C1->CR1 |= I2C_CR1_PE;						// Peripheral enable
}

