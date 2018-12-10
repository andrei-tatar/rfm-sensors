#include <Arduino.h>
#include "sensor.h"
#include "hal/lcd.h"
#include "hal/motor.h"
#include "hal/input.h"
#include "hal/sleep.h"
#include "hal/zerolcd.h"
#include "states/state.h"

void spiTransfer(uint8_t *data, uint8_t len)
{
    while (len--)
    {
        SPDR = *data;
        while (!(SPSR & (1 << SPIF)))
            ;
        *data++ = SPDR;
    }
}

Sensor sensor(spiTransfer);

void setup()
{
    DDRB = (1 << PB2) | (1 << PB1);
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPI2X);

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
