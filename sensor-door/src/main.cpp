#include <Arduino.h>
#include <sensor.h>

#define PIN_INPUT 3

Sensor sensor(false);

void setup()
{
    sensor.init();
    pinMode(PIN_INPUT, INPUT_PULLUP);
}

void loop()
{
    static uint8_t lastState = 0xFF;
    uint8_t state = !digitalRead(PIN_INPUT);
    uint16_t skipSends = 0;

    if (state != lastState || skipSends > 30 * 60)
    {
        lastState = state;
        skipSends = 0;

        uint8_t msg[4] = {'B', 0, 'S', 0};
        uint16_t voltage = sensor.readVoltage();
        msg[1] = voltage / 10 - 100;
        msg[3] = state;

        sensor.powerUp();
        if (!sensor.sendAndWait(msg, sizeof(msg)))
        {
            lastState = 0xFF;
        }
        sensor.powerDown();
    }
    else
    {
        skipSends++;
    }

    sensor.sleep(1);
}
