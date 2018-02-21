#include <Arduino.h>
#include <sensor.h>

#define LED 9

Sensor sensor(2);

void onDataReceived(const uint8_t *data, uint8_t size)
{
    Serial.print("Recevied: ");
    while (size--)
        Serial.print(*data++, HEX);
    Serial.println();
}

void setup()
{
    Serial.begin(115200);
    pinMode(LED, OUTPUT);
    sensor.onMessage(onDataReceived);
}

void loop()
{
    static uint32_t nextSend;
    if (millis() > nextSend)
    {
        nextSend = millis() + 3000;
        const char *send = "Hello from sensor";
        if (sensor.sendAndWait((uint8_t *)send, strlen(send)))
        {
            digitalWrite(LED, HIGH);
            delay(100);
            digitalWrite(LED, LOW);
        }
        else
        {
            uint8_t c = 3;
            while (c--)
            {
                digitalWrite(LED, HIGH);
                delay(100);
                digitalWrite(LED, LOW);
                delay(100);
            }
        }
    }

    sensor.update();
}
