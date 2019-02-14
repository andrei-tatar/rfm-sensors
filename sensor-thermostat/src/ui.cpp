#include "ui.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <touch.h>
#include <rtc.h>
#include "res/res.h"
#include "comm.h"

#define OLED_POWER A3

static U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0);

ThermostatState state;
bool stateRecevied = false;
uint8_t show = 0;

static void drawImage(uint8_t x, uint8_t y, const tImage &img)
{
    u8g2.drawXBMP(x, y, img.width, img.height, img.data);
}

static void drawImageCentered(const tImage &img)
{
    u8g2.firstPage();
    do
    {
        drawImage((u8g2.getWidth() - img.width) / 2,
                  (u8g2.getHeight() - img.height) / 2,
                  img_sync);
    } while (u8g2.nextPage());
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
            return img.width - 1;
        }
    }
    return 0;
}

static void drawString(uint8_t x, uint8_t y, const char *str, const tFont &font)
{
    while (*str)
    {
        x += drawChar(x, y, *str++, font);
    }
}

static void drawValue(uint8_t x, uint8_t y, uint16_t value)
{
    uint8_t v = value / 10000 % 10;
    bool show = false;
    if (v)
    {
        x += drawChar(x, y, '0' + v, roboto_big);
        show = true;
    }
    v = value / 1000 % 10;
    if (v || show)
    {
        x += drawChar(x, y, '0' + v, roboto_big);
        show = true;
    }
    v = value / 100 % 10;
    if (v || show)
    {
        x += drawChar(x, y, '0' + v, roboto_big);
        show = true;
    }
    v = value / 10 % 10;
    x += drawChar(x, y, '0' + v, roboto_big);
    y += roboto_big.chars[0].image->height -
         roboto_medium.chars[0].image->height;
    x += drawChar(x, y, '.', roboto_medium);
    drawChar(x, y, '0' + value % 10, roboto_medium);
}

static void drawValueNoDeciaml(uint8_t x, uint8_t y, uint16_t value)
{
    const tFont *font = &roboto_big;
    bool show = false;

    uint8_t v = value / 10000 % 10;
    if (v)
    {
        x += drawChar(x, y, '0' + v, *font);
        show = true;
        if (font != &roboto_medium)
        {
            font = &roboto_medium;
            y += roboto_big.chars[0].image->height -
                 roboto_medium.chars[0].image->height;
        }
    }
    v = value / 1000 % 10;
    if (show || v)
    {
        x += drawChar(x, y, '0' + v, *font);
        show = true;
        if (font != &roboto_medium)
        {
            font = &roboto_medium;
            y += roboto_big.chars[0].image->height -
                 roboto_medium.chars[0].image->height;
        }
    }
    v = value / 100 % 10;
    if (show || v)
    {
        x += drawChar(x, y, '0' + v, *font);
        show = true;
        if (font != &roboto_medium)
        {
            font = &roboto_medium;
            y += roboto_big.chars[0].image->height -
                 roboto_medium.chars[0].image->height;
        }
    }
    v = value / 10 % 10;
    if (show || v)
    {
        x += drawChar(x, y, '0' + v, *font);
        if (font != &roboto_medium)
        {
            font = &roboto_medium;
            y += roboto_big.chars[0].image->height -
                 roboto_medium.chars[0].image->height;
        }
    }

    drawChar(x, y, '0' + value % 10, *font);
}

void uiInit()
{
    pinMode(OLED_POWER, OUTPUT);
    u8g2.begin();
    touchInit();
    digitalWrite(OLED_POWER, HIGH);
}

static void drawCurrentValue()
{
    u8g2.firstPage();
    do
    {
        uint8_t y = (64 - roboto_big.chars[0].image->height) / 2;
        uint8_t yLow = y + roboto_big.chars[0].image->height -
                       roboto_medium.chars[0].image->height;
        switch (show)
        {
        case 0:

            if (state.on)
            {
                drawString(5, yLow, "Set", roboto_medium);
                drawValue(50, y, state.setpoint);
            }
            else
            {
                drawString(30, y, "Off", roboto_big);
            }
            break;
        case 1:
            drawString(10, yLow, "t", roboto_medium);
            drawValue(40, y, readTemperature);
            break;
        case 2:
            drawString(10, yLow, "H", roboto_medium);
            drawValue(40, y, readhumidity);
            break;
        case 3:
            drawString(10, yLow, "P", roboto_medium);
            drawValueNoDeciaml(40, y, readPressure);
            break;
        }

    } while (u8g2.nextPage());
}

static void syncThermostatState()
{
    drawImageCentered(img_sync);
    state.changed = false;
    stateRecevied = commGetState(state);
    if (!stateRecevied)
    {
        drawImageCentered(img_error);
    }
    else
    {
        drawCurrentValue();
    }
}

static void sendThermostatState()
{
    if (!state.changed)
    {
        return;
    }
    drawImageCentered(img_sync);
    if (!commSendState(state))
    {
        drawImageCentered(img_error);
        delay(3000);
    }
}

static void fixSetpoint()
{
    uint8_t remainder = state.setpoint % 5;
    if (remainder)
    {
        if (remainder < 3)
        {
            state.setpoint -= remainder;
        }
        else
        {
            state.setpoint += 5 - remainder;
        }
    }
}

bool uiLoop()
{
    static uint32_t nextSleep = 0;
    static uint8_t lastKey = 0xFF;
    static bool sleeping = false;

    uint8_t key = touchKey();
    if (stateRecevied && key != lastKey)
    {
        lastKey = key;

        switch (key)
        {
        case KEY_MENU:
            show += 1;
            if (show > 3)
            {
                show = 0;
            }
            break;
        case KEY_POWER:
            state.on = !state.on;
            state.changed = true;
            show = 0;
            break;
        case KEY_DOWN:
            state.changed = true;
            if (show)
            {
                show = 0;
                break;
            }
            if (!state.on)
            {
                state.on = true;
                break;
            }
            fixSetpoint();
            if (state.setpoint >= 155)
            {
                state.setpoint -= 5;
            }
            break;
        case KEY_UP:
            state.changed = true;
            if (show)
            {
                show = 0;
                break;
            }
            if (!state.on)
            {
                state.on = true;
                break;
            }
            fixSetpoint();
            if (state.setpoint <= 295)
            {
                state.setpoint += 5;
            }
            break;
        }
        drawCurrentValue();
        nextSleep = rtcTime() + 5;
    }

    if (!sleeping)
    {
        if (rtcTime() >= nextSleep)
        {
            sendThermostatState();
            touchPowerSave();
            u8g2.sleepOn();
            digitalWrite(OLED_POWER, LOW);
            sleeping = true;
        }
    }
    else
    {
        if (touchKey())
        {
            digitalWrite(OLED_POWER, HIGH);
            u8g2.sleepOff();
            touchPowerSaveOff();
            sleeping = false;

            show = 0;
            syncThermostatState();
            nextSleep = rtcTime() + (stateRecevied ? 5 : 3);
            lastKey = touchKey();
        }
    }

    return false;
}
