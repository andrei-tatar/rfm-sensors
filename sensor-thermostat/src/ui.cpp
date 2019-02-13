#include "ui.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <touch.h>
#include <rtc.h>
#include "res/res.h"

#define OLED_POWER A3

static U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0);

static void drawImage(uint8_t x, uint8_t y, const tImage &img)
{
    u8g2.drawXBMP(x, y, img.width, img.height, img.data);
}

static uint8_t drawChar(uint8_t x, uint8_t y, uint8_t ch, const tFont &font)
{
    for (uint8_t i = 0; i < font.length; i++)
    {
        const tChar &fontChar = font.chars[i];
        if (fontChar.code == ch)
        {
            const tImage &img = *fontChar.image;
            drawImage(x, y, img);
            return img.width;
        }
    }
    return 0;
}

static void drawValue(uint8_t x, uint8_t y, uint16_t value)
{
    uint8_t v = value / 1000 % 10;
    if (v)
    {
        x += drawChar(x, y, '0' + v, roboto_big);
    }
    v = value / 100 % 10;
    if (v)
    {
        x += drawChar(x, y, '0' + v, roboto_big);
    }
    v = value / 10 % 10;
    x += drawChar(x, y, '0' + v, roboto_big);
    x += drawChar(x, y, '.', roboto_medium);
    drawChar(x, y, '0' + value % 10, roboto_medium);
}

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

        static uint16_t value = 235;
        switch (key)
        {
        case KEY_DOWN:
            value -= 1;
            break;
        case KEY_UP:
            value += 1;
            break;
        }

        u8g2.firstPage();
        do
        {
            drawValue(10, (64 - roboto_big.chars[0].image->height) / 2, value);
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