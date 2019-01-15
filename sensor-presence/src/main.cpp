#include <Arduino.h>
#include <sensor.h>
#include "MAX44009.h"
#include <Wire.h>

#define PIN_INPUT 3

MAX44009 max44009;
Sensor sensor(false);

void inputStateChanged()
{
    sensor.wake();
}

void setup()
{
    pinMode(PIN_INPUT, INPUT);
    digitalWrite(A4, HIGH);
    digitalWrite(A5, HIGH);
    Wire.begin();

    sensor.init();
    if (!max44009.initialize())
    {
        sensor.powerDown();
        while (1)
        {
            sensor.sleep(10000);
        }
    }
}

void loop()
{
    uint8_t msg[7] = {'B', 0, 'L', 0, 0, 'S', 0};
    uint16_t voltage = sensor.readVoltage();
    msg[1] = voltage / 10 - 100;

    uint16_t light = max44009.getMeasurement();
    msg[3] = light >> 8;
    msg[4] = light;

    uint8_t state = digitalRead(PIN_INPUT);
    msg[6] = state;

    sensor.powerUp();
    sensor.sendAndWait(msg, sizeof(msg));
    sensor.powerDown();
    sensor.sleep(10);

    EIFR = 0x03; //clear INT0/INT1
    attachInterrupt(digitalPinToInterrupt(PIN_INPUT), inputStateChanged, RISING);
    sensor.sleep(1800); // 30 min
    detachInterrupt(digitalPinToInterrupt(PIN_INPUT));
}
