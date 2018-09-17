#include <Arduino.h>

#define PIN_POWER 10
#define PIN_ALLOW 11
#define PIN_REMOTE 12
#define PIN_VOL A0

#define IR_MUTE 0x5ea138c7
#define IR_VOL_UP 0x5ea158a7
#define IR_VOL_DOWN 0x5ea1d827
#define IR_POWER 0x7e8154ab

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
    digitalWrite(PIN_REMOTE, HIGH);

    delay(2000); //ignore ESP startup

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

void suspendIr()
{
    digitalWrite(PIN_ALLOW, LOW);
    digitalWrite(PIN_REMOTE, LOW);
    delay(10);
}

void resumeIr()
{
    delay(10);
    digitalWrite(PIN_ALLOW, HIGH);
    digitalWrite(PIN_REMOTE, HIGH);
}

void sendRaw(uint32_t code)
{
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
}

void sendRepeat()
{
    mark(9000);
    space(2250);
    mark(562);
    space(0);
}

void sendCode(uint32_t code)
{
    suspendIr();
    sendRaw(code);
    resumeIr();
}

uint8_t getVolume()
{
    return analogRead(PIN_VOL) >> 2;
}

void setVolume(uint8_t vol)
{
    suspendIr();
    uint8_t current = getVolume();
    int16_t delta = (int16_t)vol - current;
    if (abs(delta) > 2)
    {
        uint32_t lastSend = millis();
        sendRaw(delta > 0
                    ? IR_VOL_UP
                    : IR_VOL_DOWN);
        while (abs((int16_t)vol - current) > 2)
        {
            current = getVolume();
            uint32_t now = millis();
            if (now + 108 >= lastSend)
            {
                lastSend = now;
                sendRepeat();
            }
        }
    }
    delay(25);
    resumeIr();
}

void loop()
{
    switch (Serial.read())
    {
    case 'i':
        Serial.print("power: ");
        Serial.println(digitalRead(PIN_POWER), 10);
        Serial.print("vol: ");
        Serial.println(getVolume(), 10);
        Serial.print("ch: ");
        Serial.println(getActiveChannel(), 10);
        break;
    case 'p':
        sendCode(IR_POWER);
        break;
    case 'M':
        setVolume(50);
        break;
    case 'm':
        setVolume(10);
        Serial.println("ok;");
        break;
    case 's':
        digitalWrite(PIN_ALLOW, LOW);
        break;
    case 'r':
        digitalWrite(PIN_ALLOW, HIGH);
        break;
    }
}
