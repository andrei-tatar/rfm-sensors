#include "motor.h"
#include "lcd.h"
#include "input.h"
#include "sleep.h"

#define M_OPEN PE7
#define M_CLOSE PE6
#define S_ON PE2
#define S_READ PE3

#define M_STOP PORTE &= ~((1 << M_CLOSE) | (1 << M_OPEN))
#define M_START_CLOSE PORTE |= 1 << M_CLOSE
#define M_START_OPEN PORTE |= 1 << M_OPEN
#define S_TURN_ON PORTE |= 1 << PE2
#define S_TURN_OFF PORTE &= ~(1 << PE2)
#define S_VALUE ((PINE >> S_READ) & 1)

static uint16_t currentPosition;
static uint16_t maxPosition;

#define ABS_MAX 410
#define CHANGE_TIMEOUT 1000

void motorInit()
{
    DDRE |= (1 << M_CLOSE) | (1 << M_OPEN) | (1 << S_ON);
}

void motorCalibrate()
{
    LCD_writeText("ZERO");

    uint32_t lastChange = millis();
    uint8_t value = S_VALUE;
    S_TURN_ON;
    M_START_OPEN;
    while (millis() - lastChange < CHANGE_TIMEOUT)
    {
        uint8_t newValue = S_VALUE;
        if (newValue != value)
        {
            value = newValue;
            lastChange = millis();
        }
    }
    M_STOP;
    S_TURN_OFF;
    currentPosition = 0;

    LCD_writeText("INST");
    sleep();
    inputWaitPressed(BUTTON_OK);
    LCD_writeText("ADAP");

    maxPosition = ABS_MAX;
    motorTurn(100);
    maxPosition = currentPosition;
    LCD_progressbar(currentPosition, maxPosition);
}

void motorTurn(uint8_t percent)
{
    uint16_t position = percent * (uint32_t)maxPosition / 100;

    if (position > currentPosition)
    {
        uint8_t oldValue = S_VALUE;
        uint32_t lastChange = millis();
        S_TURN_ON;
        M_START_CLOSE;
        while (currentPosition < position && millis() - lastChange < CHANGE_TIMEOUT)
        {
            uint8_t newValue = S_VALUE;
            if (newValue != oldValue)
            {
                if (newValue)
                {
                    currentPosition++;
                    LCD_progressbar(currentPosition, maxPosition);
                    lastChange = millis();
                }
                oldValue = newValue;
            }
        }
    }
    else
    {
        uint8_t oldValue = S_VALUE;
        uint32_t lastChange = millis();
        S_TURN_ON;
        M_START_OPEN;
        while (currentPosition > position && millis() - lastChange < CHANGE_TIMEOUT)
        {
            uint8_t newValue = S_VALUE;
            if (newValue != oldValue)
            {
                if (!newValue)
                {
                    currentPosition--;
                    LCD_progressbar(currentPosition, maxPosition);
                    lastChange = millis();
                }
                oldValue = newValue;
            }
        }
    }

    M_STOP;
    S_TURN_OFF;
}

uint16_t motorPosition()
{
    return currentPosition;
}

uint16_t motorMaxPosition()
{
    return maxPosition;
}
