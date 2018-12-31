#include <Arduino.h>
#include "package.h"

#define FRAME_HEADER_1 0xDE
#define FRAME_HEADER_2 0x5B
#define CHKSUM_INIT 0x1021
#define MAX_PCKG_SIZE 64

void pckgInit()
{
    Serial.begin(57600);
}

void updateChecksum(uint16_t &checksum, uint8_t data)
{
    bool roll = (checksum & 0x8000) != 0 ? true : false;
    checksum <<= 1;
    checksum &= 0xFFFF;
    if (roll)
    {
        checksum |= 1;
    }
    checksum ^= data;
}

void pckgLoop()
{
    static uint8_t status = 0;
    static uint32_t last = 0;
    static uint8_t pckg[MAX_PCKG_SIZE];
    static uint8_t pckgSize;
    static uint8_t pckgOffset;
    static uint16_t pckgChksum;

    uint32_t now = millis();

    if (status && now - last > 300)
    {
        status = 0;
    }

    while (Serial.available())
    {
        uint8_t data = Serial.read();

        switch (status)
        {
        case 0:
            if (data == FRAME_HEADER_1)
                status = 1;
            break;
        case 1:
            if (data == FRAME_HEADER_2)
                status = 2;
            break;
        case 2:
            pckgSize = data;
            pckgChksum = CHKSUM_INIT;
            if (pckgSize > MAX_PCKG_SIZE)
            {
                status = 0;
                break;
            }
            updateChecksum(pckgChksum, pckgSize);
            pckgOffset = 0;
            status = 3;
        case 3:
            pckg[pckgOffset++] = data;
            updateChecksum(pckgChksum, data);
            if (pckgOffset == pckgSize)
            {
                status = 4;
            }
            break;
        case 4:
            if (pckgChksum >> 8 == data)
            {
                status = 5;
            }
            else
            {
                status = 0;
            }
            break;
        case 5:
            if (pckgSize && (pckgChksum & 0xFF) == data)
            {
                pckgReceived(pckg, pckgSize);
            }
            status = 0;
            break;
        }
    }
}

void pckgSend(uint8_t *data, uint8_t length)
{
    uint16_t chksum = CHKSUM_INIT;
    Serial.write(FRAME_HEADER_1);
    Serial.write(FRAME_HEADER_2);

    Serial.write(length);
    updateChecksum(chksum, length);

    while (length--)
    {
        Serial.write(*data);
        updateChecksum(chksum, *data);
        data++;
    }

    Serial.write(chksum >> 8);
    Serial.write(chksum & 0xFF);
}
