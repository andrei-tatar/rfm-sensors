/*
 * hal.h
 *
 *  Created on: Apr 15, 2013
 *      Author: User
 */

#ifndef HAL_H_
#define HAL_H_

#include <stdint.h>
#include "Cpu.h"
#include "spi.h"
#include "serial.h"
#include "PORT_PDD.h"

inline void interrupts() { __EI(); }
inline void noInterrupts() { __DI(); }

void delay(uint16_t msec);
uint32_t millis();

#endif /* HAL_H_ */
