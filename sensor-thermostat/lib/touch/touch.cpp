#include "touch.h"
#include <Wire.h>

#define CHANGE_PIN 3

static volatile uint8_t key = 0;
static uint8_t touchReadKey();

void touchChangeISR()
{
    key = 0xFF;
}

void touchInit()
{
    // reset device by writing non-zero value to register 0x39
    Wire.beginTransmission(0x1B); // transmit to device
    Wire.write(0x39);             // sets register pointer to the reset register (0x39)
    Wire.write(0xFF);             // send non-zero value to initiate a reset
    Wire.endTransmission();       // stop transmitting
    delay(100);                   // wait for device to restart

    digitalWrite(CHANGE_PIN, 1); //pull up
    attachInterrupt(digitalPinToInterrupt(CHANGE_PIN), touchChangeISR, FALLING);
}

void touchPowerSave()
{
    Wire.beginTransmission(0x1B);
    Wire.write(54);         //power reg
    Wire.write(64);         //~500 msec
    Wire.endTransmission(); //stop transmitting
}

void touchPowerSaveOff()
{
    Wire.beginTransmission(0x1B);
    Wire.write(54); //power reg
    Wire.write(2);
    Wire.endTransmission(); //stop transmitting
}

uint8_t touchKey()
{
    uint8_t pressedKey = key;
    if (pressedKey == 0xFF || digitalRead(CHANGE_PIN) == 0)
    {
        int readstatus = 0;
        int bttnNum = 0;

        Wire.beginTransmission(0x1B);
        Wire.write(0x02);
        Wire.endTransmission(false);
        Wire.requestFrom(0x1B, 1);
        readstatus = Wire.read();

        Wire.beginTransmission(0x1B);
        Wire.write(0x03);
        Wire.endTransmission(false);
        Wire.requestFrom(0x1B, 1);
        bttnNum = Wire.read();

        if (readstatus == 1)
        {
            pressedKey = bttnNum & 0x0F;
        }
        else
        {
            pressedKey = 0;
        }
    }
    return key = pressedKey;
}
