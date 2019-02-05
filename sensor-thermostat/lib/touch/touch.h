#ifndef _TOUCH_H_
#define _TOUCH_H_

#include <Arduino.h>

#define KEY_POWER 0x08
#define KEY_DOWN 0x04
#define KEY_UP 0x02
#define KEY_MENU 0x01

void touchInit();
uint8_t touchKey();
void touchPowerSave();
void touchPowerSaveOff();

#endif