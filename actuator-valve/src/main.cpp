#include <Arduino.h>
#include "hal/lcd.h"
#include "hal/motor.h"
#include "hal/input.h"
#include "hal/sleep.h"
#include "hal/zerolcd.h"

#include "states/state.h"

void setup()
{
    motorInit();
    inputInit();
    initLCD();
    motorCalibrate();

    Lcd_Symbol(RADIO, 1);
    Lcd_Symbol(AUTO, 1);
}

void loop()
{
    bool skipSleep = handleState();
    skipSleep |= motorTick();
    if (!skipSleep)
    {
        sleep();
    }
}
