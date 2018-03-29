#include <Arduino.h>

#define PIN_POWER 10
#define PIN_ALLOW 11
#define PIN_REMOTE 12
#define PIN_VOL A0

void setup()
{
    for (uint8_t ch = 0; ch < 8; ch++)
    {
        pinMode(2 + ch, INPUT);
    }
    pinMode(PIN_POWER, INPUT);
    pinMode(PIN_ALLOW, OUTPUT);
    digitalWrite(PIN_ALLOW, HIGH);
    pinMode(PIN_REMOTE, OUTPUT);
    digitalWrite(PIN_REMOTE, LOW);
    Serial.begin(57600);
}

uint8_t getActiveChannel()
{
    for (uint8_t ch = 0; ch < 8; ch++)
    {
        if (digitalRead(2 + ch) == LOW)
        {
            return ch + 1;
        }
    }
    return 0;
}

void mark(uint16_t time)
{
    digitalWrite(PIN_REMOTE, HIGH);
    delayMicroseconds(time);
}

void space(uint16_t time)
{
    digitalWrite(PIN_REMOTE, LOW);
    delayMicroseconds(time);
}

void sendCode(uint32_t code)
{
    digitalWrite(PIN_ALLOW, LOW);
    delay(10);

    mark(9000);
    space(4500);

    for (unsigned long mask = 1UL << 31; mask; mask >>= 1)
    {
        mark(562);
        if (code & mask)
            space(1687);
        else
            space(562);
    }

    mark(562);
    space(0);

    delay(5);
    digitalWrite(PIN_ALLOW, HIGH);
}

void loop()
{
    switch (Serial.read())
    {
    case 'i':
        Serial.print("power: ");
        Serial.println(digitalRead(PIN_POWER), 10);
        Serial.print("vol: ");
        Serial.println(analogRead(PIN_VOL), 10);
        Serial.print("ch: ");
        Serial.println(getActiveChannel(), 10);
        break;
    case 'p':
        sendCode(0x7e8154ab);
        break;
    }
}
