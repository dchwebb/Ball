#include "uartHandler.h"

volatile uint8_t uartCmdPos = 0;
volatile char uartCmd[100];
volatile bool uartCmdRdy = false;

// Manages communication to ST Link debugger UART
void InitUart() {
	// MODER 00: Input mode, 01: General purpose output mode, 10: Alternate function mode, 11: Analog mode (reset state)

    // PB6: USART1_TX; PB7 USART1_RX
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

	GPIOB->MODER &= ~GPIO_MODER_MODE6_0;			// Set alternate function on PB6
	GPIOB->AFR[0] |= 7 << GPIO_AFRL_AFSEL6_Pos;		// Alternate function for USART1_TX is AF7
	GPIOB->MODER &= ~GPIO_MODER_MODE7_0;			// Set alternate function on PB7
	GPIOB->AFR[0] |= 7 << GPIO_AFRL_AFSEL7_Pos;		// Alternate function for USART1_RX is AF7

	// By default clock source is muxed to peripheral clock 2 which is system clock (change clock source in RCC->CCIPR1)
	// Calculations depended on oversampling mode set in CR1 OVER8. Default = 0: Oversampling by 16
	int USARTDIV = (SystemCoreClock * 2) / 115200;		// clk / desired_baud
	USART1->BRR = USARTDIV & ~8;					// BRR[3] must not be set set to 0x226
	USART1->CR1 &= ~USART_CR1_M;					// 0: 1 Start bit, 8 Data bits, n Stop bit; 	1: 1 Start bit, 9 Data bits, n Stop bit
	USART1->CR1 |= USART_CR1_RE;					// Receive enable
	USART1->CR1 |= USART_CR1_TE;					// Transmitter enable
	USART1->CR1 |= USART_CR1_OVER8;					// Oversampling by 8 (default is 16)

	// Set up interrupts
	USART1->CR1 |= USART_CR1_RXNEIE;
	NVIC_SetPriority(USART1_IRQn, 3);				// Lower is higher priority
	NVIC_EnableIRQ(USART1_IRQn);

	USART1->CR1 |= USART_CR1_UE;					// USART Enable

}


std::string IntToString(const int32_t& v) {
	return std::to_string(v);
}

std::string HexToString(const uint32_t& v, const bool& spaces) {
	char buf[20];
	if (spaces) {
		if (v != 0) {
			sprintf(buf, "%X %X %X %X", (unsigned int)(v & 0xFF), (unsigned int)(v >> 8) & 0xFF, (unsigned int)(v >> 16) & 0xFF, (unsigned int)(v >> 24) & 0xFF);
		} else {
			sprintf(buf, " ");
		}
	} else {
		sprintf(buf, "%X", (unsigned int&)v);
	}
	return std::string(buf);
}

char uartBuf[100];
std::string HexToString(const uint8_t* v, uint32_t len, const bool spaces) {
	uint32_t pos = 0, cnt = 0;
	len = std::min(50ul, len);
//	while (len > 0) {
//		pos += sprintf(&uartBuf[pos], (spaces ? "%02X ": "%02X"), v[cnt++]);
//		len--;
//	}

	for (uint8_t i = 0; i < len; ++i) {
		pos += sprintf(&uartBuf[pos], "%02X ", v[cnt++]);
	}

	return std::string(uartBuf, len*3);
}


std::string HexByte(const uint16_t& v) {
	char buf[50];
	sprintf(buf, "%02X", v);
	return std::string(buf);

}

void uartSendChar(char c) {
	while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0);
	USART1->TDR = c;
}


size_t uartSendString(const unsigned char* s, size_t size) {
	for (size_t i = 0; i < size; ++i) {
		while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0);
		USART1->TDR = s[i];
	}
	return size;
}

void uartSendString(const char* s) {
	char c = s[0];
	uint8_t i = 0;
	while (c) {
		while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0);
		USART1->TDR = c;
		c = s[++i];
	}
}

void uartSendString(const std::string& s) {
	for (char c : s) {
		while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0);
		USART1->TDR = c;
	}
}

extern "C" {

// USART Decoder
void USART1_IRQHandler() {
	if (!uartCmdRdy) {
		uartCmd[uartCmdPos] = USART1->RDR; 				// accessing RDR automatically resets the receive flag
		if (uartCmd[uartCmdPos] == 10) {
			uartCmdRdy = true;
			uartCmdPos = 0;
		} else {
			uartCmdPos++;
		}
	}
}

//// To enable UART for printf commands
//size_t _write(int handle, const unsigned char* buf, size_t bufSize)
//{
//	return uartSendString(buf, bufSize);
//}
}
