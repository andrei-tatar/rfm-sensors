#include <Arduino.h>
#include <sensor.h>

#define LED 9

Sensor sensor(2);

void onDataReceived(const uint8_t *data, uint8_t size)
{
    digitalWrite(LED, HIGH);
    digitalWrite(LED, LOW);
}

void setup()
{
    const uint8_t key[16] = {0xd9, 0xc1, 0xbd, 0x60, 0x9c, 0x35, 0x3e, 0x8b, 0xab, 0xbc, 0x8d, 0x35, 0xc7, 0x04, 0x74, 0xef};
    sensor.init(key);
    sensor.onMessage(onDataReceived);
    pinMode(LED, OUTPUT);
}

void loop()
{
    static uint32_t nextSend;
    if (millis() > nextSend)
    {
        nextSend = millis() + 2000;
        sensor.send((uint8_t *)"Hello", 5);
    }

    sensor.update();
}
