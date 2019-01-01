#include <Arduino.h>
#include "package.h"

#define PIN_POWER 10
#define PIN_ALLOW 11
#define PIN_REMOTE 12
#define PIN_VOL A0

#define IR_MUTE 0x5ea138c7
#define IR_VOL_UP 0x5ea158a7
#define IR_VOL_DOWN 0x5ea1d827
#define IR_POWER 0x7e8154ab

uint32_t channelMap[8] = {
    0x5ea103fc, // line 3
    0x5ea1837c, // line 2
    0x5ea19867, // line 1
    0x5ea1ca34, // optical
    0x5ea118e7, // coaxial
    0x5ea1a857, // CD
    0x5ea16897, // tuner
    0x5ea128d7, // phono
};

static bool forceSend = true;

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
    pckgInit();
}

uint8_t getActiveChannel()
{
    if (!digitalRead(PIN_POWER))
    {
        return 0;
    }

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
        uint32_t now = millis();
        uint32_t lastSend = now;
        uint32_t start = now;
        sendRaw(delta > 0
                    ? IR_VOL_UP
                    : IR_VOL_DOWN);

        while (abs((int16_t)vol - current) > 2)
        {
            current = getVolume();
            now = millis();
            if (now - start > 5000 || !digitalRead(PIN_POWER))
            {
                break;
            }
            if (now - lastSend >= 108)
            {
                lastSend = now;
                sendRepeat();
            }
        }
    }
    delay(25);
    resumeIr();
}

void pckgReceived(uint8_t *data, uint8_t length)
{
    switch (data[0])
    {
    case 0x01:
        forceSend = true;
        break;
    case 0xE0:
        if (!data[1] != !digitalRead(PIN_POWER))
        {
            sendCode(IR_POWER);
        }
        break;
    case 0xCA:
        if (getActiveChannel() != data[1] && data[1] < sizeof(channelMap))
        {
            sendCode(channelMap[data[1] - 1]);
        }
        break;
    case 0x10:
        setVolume(data[1]);
        break;
    }
}

void loop()
{
    pckgLoop();

    static bool lastPwr = 0xff;
    static uint8_t lastChannel = 0xff;
    static uint8_t lastVol = 0xff;
    static uint32_t lastCheck = 0;

    uint32_t now = millis();
    if (now - lastCheck > 300 || forceSend)
    {
        lastCheck = now;

        bool pwr = digitalRead(PIN_POWER);
        uint8_t channel = getActiveChannel();
        uint8_t volume = getVolume();

        if (pwr && channel == 0)
        {
            channel = lastChannel;
        }

        if (forceSend ||
            abs((int16_t)volume - (int16_t)lastVol) > 2 ||
            pwr != lastPwr || channel != lastChannel)
        {
            forceSend = false;
            lastPwr = pwr;
            lastChannel = channel;
            lastVol = volume;
            uint8_t status[4] = {0x01, pwr, channel, volume};
            pckgSend(status, sizeof(status));
        }
    }
}
