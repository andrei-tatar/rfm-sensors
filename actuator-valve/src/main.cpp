#include <Arduino.h>
#include "hal/lcd.h"
#include "hal/motor.h"
#include "hal/input.h"
#include "hal/sleep.h"
#include "hal/zerolcd.h"
#include "hal/rtc.h"
#include "states/state.h"
#include "comm.h"

void setup()
{
    motorInit();
    inputInit();
    initLCD();
    rtcInit();
    commInit();
    motorCalibrate();
    commStart();
    Lcd_Symbol(AUTO, 1);
}

void loop()
{
    bool skipSleep = handleState();
    skipSleep |= motorTick();
    skipSleep |= commUpdate();
    if (!skipSleep)
    {
        sleep();
    }
}
