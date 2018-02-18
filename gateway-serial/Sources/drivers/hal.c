/*
 * hal.c
 *
 *  Created on: Apr 15, 2013
 *      Author: User
 */

#include "hal.h"

volatile uint32_t time = 0;

void delay(uint16_t msec)
{
	register uint32_t endTime = time + msec;
	while (endTime > time);
}

void delay_us(uint32_t usec)
{
	while (usec--)
	{
		PE_NOP();PE_NOP();PE_NOP();PE_NOP();PE_NOP();PE_NOP();PE_NOP();PE_NOP();
		PE_NOP();PE_NOP();PE_NOP();PE_NOP();PE_NOP();PE_NOP();PE_NOP();PE_NOP();PE_NOP();
	}
}

PE_ISR(sysTickTimerInterrupt)
{
	time += 1;
}

uint32_t millis()
{
	return time;
}
