#include "Arduino.h"
#include "sensor.h"
#include "hal/lcd.h"
#include "hal/zerolcd.h"
#include "hal/rtc.h"
#include "hal/motor.h"

#define POLL_SECONDS 300
#define POLL_FAIL_SECONDS 10
#define POLL_ALIVE_MS 100

void spiTransfer(uint8_t *data, uint8_t len);
static Sensor sensor(spiTransfer);
static bool received = false;
static uint8_t lastRssi = 0;
static uint16_t temperature = 0;
static uint32_t lastSendRtc;
static uint16_t battery = 0;

void spiTransfer(uint8_t *data, uint8_t len)
{
    PORTF &= ~(1 << PF7);
    while (len--)
    {
        SPDR = *data;
        while (!(SPSR & (1 << SPIF)))
        {
        }
        *data++ = SPDR;
    }
    PORTF |= (1 << PF7);
}

static void readBattery()
{
    ADMUX = (1 << REFS0) | 30;
    ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADPS2);
    while (ADCSRA & (1 << ADSC))
    {
    }
    uint16_t adc = ADC;
    uint16_t newBatteryVoltage = 1100 * 1024UL / adc; //mV
    if (battery != 0)
    {
        battery = (battery + newBatteryVoltage) / 2;
    }
    else
    {
        battery = newBatteryVoltage;
    }
    ADCSRA &= ~(1 << ADEN);
}

void messageReceived(const uint8_t *data, uint8_t length, uint8_t rssi)
{
    received = true;
    sensor.powerDown();

    if (lastRssi != 0)
    {
        lastRssi = (lastRssi + rssi) / 2;
    }
    else
    {
        lastRssi = rssi;
    }
    if (data[0] == 0xDE && length == 4)
    {
        motorTurn(data[1]);
        temperature = data[2] << 8 | data[3];
    }
}

uint16_t commTemperature()
{
    return temperature;
}

uint8_t commLastRssi()
{
    return lastRssi;
}

void commInit()
{
    //init SPI
    DDRB = (1 << PB1) | (1 << PB2) | (1 << PB0);
    PORTB |= 1 << PB0;
    PORTF |= 1 << PF7;
    DDRF = (1 << PF7);
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPI2X);

    if (sensor.init())
    {
        Lcd_Symbol(RADIO, 1);
    }
    sensor.powerLevel(31);
    sensor.powerDown();
    sensor.onMessage(messageReceived);
}

void commStart()
{
    lastSendRtc = rtcTime() - POLL_SECONDS - 3;
}

uint16_t commNextTry()
{
    return POLL_SECONDS - (rtcTime() - lastSendRtc);
}

uint16_t commBattery()
{
    return battery;
}

bool commUpdate()
{
    static bool sending = false;
    static uint32_t lastSendMillis;

    if (sending)
    {
        sensor.update();
        if (!received)
        {
            if (millis() - lastSendMillis >= POLL_ALIVE_MS)
            {
                sending = false;
                sensor.powerDown();
                lastSendRtc -= POLL_SECONDS - POLL_FAIL_SECONDS;
            }
        }
        else
        {
            sending = false;
        }
    }
    else
    {
        uint32_t now = rtcTime();
        if (now - lastSendRtc >= POLL_SECONDS)
        {
            lastSendRtc = now;
            sending = true;
            readBattery();

            sensor.powerUp();
            lastSendMillis = millis();
            received = false;

            uint8_t sendBat = battery / 10 - 100;
            uint8_t reqData[2] = {'B', sendBat};
            sending = sensor.send(reqData, sizeof(reqData));
        }
    }

    return sending;
}
