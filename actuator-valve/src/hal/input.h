#ifndef __INPUT_H__
#define __INPUT_H__

#include <Arduino.h>

#define BUTTON_OK PB6
#define BUTTON_MENU PB4
#define BUTTON_TIME PB5

void inputInit();
bool inputPressed(uint8_t btn);
void inputWaitPressed(uint8_t btn);

#endif
