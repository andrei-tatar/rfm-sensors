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

static volatile uint16_t currentPosition = 0;
static volatile uint16_t targetPosition = 0;
static volatile uint32_t lastPositionChange;
static volatile int8_t delta = 0;
static volatile uint16_t maxPosition;
static volatile bool motorRunning = false;

#define ABS_MAX 410
#define CHANGE_TIMEOUT 500

void motorInit()
{
    DDRE |= (1 << M_CLOSE) | (1 << M_OPEN) | (1 << S_ON);
    EIMSK |= 1 << PCIE0;
    PCMSK0 |= 1 << PCINT3;
}

ISR(PCINT0_vect)
{
    if (delta == 0)
    {
        return;
    }
    if (delta == 1)
    {
        if (S_VALUE)
        {
            currentPosition++;
        }
    }
    else if (delta == -1)
    {
        if (!S_VALUE)
        {
            if (currentPosition)
                currentPosition--;
        }
    }
    lastPositionChange = millis();
    LCD_progressbar(currentPosition, maxPosition);
    if (currentPosition == targetPosition)
    {
        M_STOP;
    }
}

void motorCalibrate()
{
    LCD_writeText("ZERO");

    currentPosition = ABS_MAX + 50;
    maxPosition = currentPosition;
    motorTurn(0);
    //wait for motor to stop to determine zero
    while (motorTick())
    {
    }

    LCD_progressbar(0, 100);
    LCD_writeText("INST");
    inputWaitPressed(BUTTON_OK);
    LCD_writeText("ADAP");

    currentPosition = 0;
    maxPosition = ABS_MAX;
    motorTurn(100);
    //wait for motor to stop to determine max position
    while (motorTick())
    {
    }
    maxPosition = currentPosition;
    LCD_progressbar(currentPosition, maxPosition);
}

void motorTurn(uint8_t percent)
{
    if (percent > 100)
        percent = 100;
    targetPosition = (uint32_t)percent * (uint32_t)maxPosition / 100UL;
    if (targetPosition == currentPosition)
    {
        return;
    }

    if (motorRunning)
    {
        M_STOP;
        delay(1000);
    }
    delta = 0;
    S_TURN_ON;
    lastPositionChange = millis();
    if (targetPosition > currentPosition)
    {
        delta = 1;
        M_START_CLOSE;
    }
    else
    {
        delta = -1;
        M_START_OPEN;
    }
    motorRunning = true;
}

bool motorTick()
{
    if (motorRunning)
    {
        cli();
        uint32_t lastChange = lastPositionChange;
        sei();
        if (millis() - lastChange < CHANGE_TIMEOUT)
        {
            return true;
        }
        M_STOP;
        delta = 0;
        S_TURN_OFF;
        motorRunning = false;
    }
    return false;
}

uint16_t motorPosition()
{
    cli();
    uint16_t pos = currentPosition;
    sei();
    return pos;
}

uint16_t motorMaxPosition()
{
    return maxPosition;
}
