#include <Arduino.h>
#include <rtc.h>
#include <avr/sleep.h>
#include "ui.h"
#include "comm.h"

void setup()
{
    rtcInit();
    commInit();
    uiInit();
}

void loop()
{
    bool skipSleep = uiLoop();
    skipSleep |= commLoop();
    if (skipSleep)
    {
        return;
    }

    ADCSRA &= ~(1 << ADEN);
    set_sleep_mode(SLEEP_MODE_PWR_SAVE);
    sleep_mode();
}
