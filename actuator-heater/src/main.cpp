#include <Arduino.h>
#include <sensor.h>
#include <FastLED.h>

Sensor sensor(false);
CRGB led;
volatile uint32_t lastReceive = 0;
volatile bool fail = false;

#define PIN_CMD 3
#define PIN_LED 6
#define TIMEOUT 300000 // 5min

void onData(const uint8_t *data, uint8_t length, uint8_t rssi)
{
    if (length == 1)
    {
        fail = false;
        digitalWrite(PIN_CMD, *data);
        lastReceive = millis();
        FastLED.showColor(CRGB(0, *data ? 5 : 0, 5));
    }
}

void setup()
{
    sensor.init();
    sensor.onMessage(onData);

    pinMode(PIN_CMD, OUTPUT);
    digitalWrite(PIN_CMD, LOW);

    uint8_t here = 1;
    sensor.send(&here, 1);

    FastLED.addLeds<WS2812, PIN_LED, GRB>(&led, 1);
}

void loop()
{

    if (!fail && (lastReceive == 0 || millis() - lastReceive > TIMEOUT))
    {
        FastLED.showColor(CRGB(5, 0, 0));
        digitalWrite(PIN_CMD, LOW);
        fail = true;
    }

    sensor.update();
}
