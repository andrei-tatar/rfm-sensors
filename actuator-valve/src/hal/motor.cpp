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
#define M_RUNNING (PORTE & ((1 << M_CLOSE) | (1 << M_OPEN)))

static volatile uint16_t currentPosition;
static volatile uint16_t targetPosition;
static volatile uint32_t lastPositionChange;
static volatile int8_t delta = 0;
static uint16_t maxPosition;

#define ABS_MAX 410
#define CHANGE_TIMEOUT 500

void motorInit()
{
    DDRE |= (1 << M_CLOSE) | (1 << M_OPEN) | (1 << S_ON);
    EIMSK |= 1 << PCIE0;
    PCMSK0 |= 1 << PCINT3;
}

void motorStop()
{
    M_STOP;
    S_TURN_OFF;
    delta = 0;
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
            lastPositionChange = millis();
        }
    }
    else if (delta == -1)
    {
        if (!S_VALUE)
        {
            currentPosition--;
            lastPositionChange = millis();
        }
    }
    LCD_progressbar(currentPosition, maxPosition);
    if (currentPosition == targetPosition)
    {
        motorStop();
    }
}

void motorCalibrate()
{
    LCD_writeText("ZERO");

    lastPositionChange = millis();
    S_TURN_ON;
    M_START_OPEN;
    delta = 1;
    while (millis() - lastPositionChange < CHANGE_TIMEOUT)
    {
    }
    M_STOP;
    S_TURN_OFF;
    delta = 0;
    currentPosition = 0;

    LCD_writeText("INST");
    sleep();
    inputWaitPressed(BUTTON_OK);
    LCD_writeText("ADAP");

    maxPosition = ABS_MAX;
    motorTurn(100);
    //wait for motor to stop to determine max position
    while (M_RUNNING)
    {
        motorTick();
    }
    maxPosition = currentPosition;
    LCD_progressbar(currentPosition, maxPosition);
}

void motorTurn(uint8_t percent)
{
    if (percent > 100)
        percent = 100;
    targetPosition = percent * (uint32_t)maxPosition / 100;

    lastPositionChange = millis();
    if (M_RUNNING)
    {
        M_STOP;
        delay(CHANGE_TIMEOUT);
    }
    S_TURN_ON;
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
}

void motorTick()
{
    if (M_RUNNING && millis() - lastPositionChange > CHANGE_TIMEOUT)
    {
        motorStop();
    }
}

uint16_t motorPosition()
{
    return currentPosition;
}

uint16_t motorMaxPosition()
{
    return maxPosition;
}
