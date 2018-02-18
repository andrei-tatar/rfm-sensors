/*
 * spi.c
 *
 *  Created on: Jan 25, 2014
 *      Author: Andrei
 */

#include "spi.h"

#define CS_PIN	(1 << 5)

static uint8_t spi_ReadWriteByte(uint8_t data)
{
	while (!(SPI0_BASE_PTR->S & SPI_S_SPTEF_MASK)); //wait TX to be empty
	SPI0_BASE_PTR->D = data;
	while (!(SPI0_BASE_PTR->S & SPI_S_SPRF_MASK)); //wait RX to be full
	return SPI0_BASE_PTR->D;
}

void spi_Transfer(uint8_t* data, uint8_t len) {
	PTD_BASE_PTR->PCOR = CS_PIN;
	
	while (len--) {
		*data = spi_ReadWriteByte(*data);
		data++;
	}
	
	PTD_BASE_PTR->PSOR = CS_PIN;
}
