#include "ui.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <touch.h>
#include <rtc.h>

#define OLED_POWER A3

static U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0);

void uiInit()
{
    pinMode(OLED_POWER, OUTPUT);
    u8g2.begin();
    touchInit();
    digitalWrite(OLED_POWER, HIGH);
}

bool uiLoop()
{
    static uint32_t nextSleep = 5;
    static uint8_t lastKey = 0xFF;
    static bool skipSleep = true;

    uint8_t key = touchKey();
    if (key != lastKey)
    {
        lastKey = key;
        char str[2];
        str[0] = key % 10 + '0';
        str[1] = 0;

        u8g2.firstPage();
        do
        {
            u8g2.setFont(u8g2_font_ncenB14_tr);
            u8g2.drawStr(0, 24, str);
        } while (u8g2.nextPage());
        nextSleep = rtcTime() + 5;
    }

    if (skipSleep)
    {
        if (rtcTime() >= nextSleep)
        {
            touchPowerSave();
            u8g2.sleepOn();
            digitalWrite(OLED_POWER, LOW);
            skipSleep = false;
        }
    }
    else
    {
        if (touchKey())
        {
            digitalWrite(OLED_POWER, HIGH);
            u8g2.sleepOff();
            touchPowerSaveOff();
            skipSleep = true;
        }
    }

    return skipSleep;
}