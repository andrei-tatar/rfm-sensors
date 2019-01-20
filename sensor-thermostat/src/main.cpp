#include <Arduino.h>
#include <sensor.h>
#include <Adafruit_BME280.h>
#include <rtc.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <avr/sleep.h>

Adafruit_BME280 bme;
Sensor sensor(false);

#define OLED_POWER A3

U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0);

void setup()
{
    rtcInit();
    sensor.init();
    sensor.powerLevel(31);

    Wire.begin();
    pinMode(OLED_POWER, OUTPUT);
    digitalWrite(OLED_POWER, HIGH);
    u8g2.begin();

    u8g2.firstPage();
    do
    {
        u8g2.setFont(u8g2_font_ncenB14_tr);
        u8g2.drawStr(0, 24, "Hello World!");
    } while (u8g2.nextPage());
}

void loop()
{
    static uint32_t last = 0;

    if (last != rtcTime())
    {
        last = rtcTime();
        sensor.send((uint8_t *)&last, 4);
    }
    sensor.update();

    if (rtcTime() == 5)
    {
        digitalWrite(OLED_POWER, LOW);
        pinMode(A4, OUTPUT);
        digitalWrite(A4, 0);
        pinMode(A5, OUTPUT);
        digitalWrite(A5, 0);

        sensor.powerDown();
        ADCSRA &= ~(1 << ADEN);
        while (true)
        // while (rtcTime() <= 15)
        {
            set_sleep_mode(SLEEP_MODE_PWR_SAVE);
            sleep_mode();
        }
        sensor.powerUp();

        pinMode(A4, INPUT);
        pinMode(A5, INPUT);
        digitalWrite(OLED_POWER, HIGH);
        u8g2.begin();

        u8g2.firstPage();
        do
        {
            u8g2.setFont(u8g2_font_ncenB14_tr);
            u8g2.drawStr(0, 24, "Hello World!");
        } while (u8g2.nextPage());
    }
}
