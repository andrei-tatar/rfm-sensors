#ifndef __MOTOR_H__
#define __MOTOR_H__

#include <Arduino.h>

void motorInit();
void motorCalibrate();
void motorTurn(uint8_t position);
bool motorTick();

uint16_t motorPosition();
uint16_t motorMaxPosition();

#endif
