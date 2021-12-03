#include "compassHandler.h"
#include "initialisation.h"

void I2CSendData()
{
	// 7-bit address (0x1E) plus 1 bit read/write identifier, i.e. 0x3D for read and 0x3C for write
	I2C1->CR2 |= 0x3C & I2C_CR2_SADD_Msk;
	I2C1->CR2 &= ~I2C_CR2_RD_WRN;				// 0: Master requests a write transfer; 1: Master requests a read transfer
	//I2C1->CR2 |= I2C_CR2_RD_WRN;				// 0: Master requests a write transfer; 1: Master requests a read transfer
	I2C1->CR2 |= 2 << I2C_CR2_NBYTES_Pos;		// Send 2 bytes
	I2C1->CR2 |= I2C_CR2_START;					// Will continue sending address until ACK

	while ((I2C1->ISR & I2C_ISR_TXIS) == 0);	// Wait until ready
	I2C1->TXDR = 0x01;							// CRB_REG_M register to set guass scale
	while ((I2C1->ISR & I2C_ISR_TXIS) == 0);	// Wait until ready
	I2C1->TXDR = 0x20;							// SEt Sensor input	field range to +-1.3 guass

	while ((I2C1->ISR & I2C_ISR_TC) == 0);		// Wait until transmit complete
	volatile int x __attribute__((unused)) = 1;
}
