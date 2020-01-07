#include <Arduino.h>
#include <sensor.h>

#define PIN_INPUT 3

Sensor sensor(false);

void inputStateChanged()
{
    sensor.wake();
}

void setup()
{
    sensor.init();
    pinMode(PIN_INPUT, INPUT);
    digitalWrite(PIN_INPUT, HIGH);
    attachInterrupt(digitalPinToInterrupt(PIN_INPUT), inputStateChanged, CHANGE);
}

void loop()
{
    uint8_t msg[4] = {'B', 0, 'S', 0};
    uint16_t voltage = sensor.readVoltage();
    msg[1] = voltage / 10 - 100;

    uint8_t state = !digitalRead(PIN_INPUT);
    msg[3] = state;

    uint16_t sleep = 1800;
    sensor.powerUp();
    sleep = sensor.sendAndWait(msg, sizeof(msg))
        ? 1800
        : 1;
    sensor.powerDown();

    sensor.sleep(sleep);
}
