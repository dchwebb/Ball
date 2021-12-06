#pragma once

#include "initialisation.h"

void I2CSetup();
void I2CStartRead();
void I2CWriteCmd(uint8_t reg, uint8_t val);
void I2CWriteAddr(uint8_t reg);
uint8_t I2CReadData();
void I2CProcessResults();
