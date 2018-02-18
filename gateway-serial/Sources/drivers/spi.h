/*
 * spi.h
 *
 *  Created on: Jan 25, 2014
 *      Author: Andrei
 */

#ifndef SPI_H_
#define SPI_H_

#include <stdint.h>
#include "SPI0.h"
#include "hal.h"

void spi_Transfer(uint8_t* data, uint8_t len);

#endif /* SPI_H_ */
