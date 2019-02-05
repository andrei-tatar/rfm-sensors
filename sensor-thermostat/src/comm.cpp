#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <sensor.h>
#include <rtc.h>

static Adafruit_BME280 bme;
Sensor sensor(false);

void commInit()
{
    sensor.init();
    sensor.powerLevel(31);
    sensor.powerDown();
    bme.begin(0x76);
    bme.setSampling(
        Adafruit_BME280::sensor_mode::MODE_NORMAL,
        Adafruit_BME280::sensor_sampling::SAMPLING_X16,
        Adafruit_BME280::sensor_sampling::SAMPLING_X16,
        Adafruit_BME280::sensor_sampling::SAMPLING_X16,
        Adafruit_BME280::sensor_filter::FILTER_X4,
        Adafruit_BME280::standby_duration::STANDBY_MS_1000);
}

bool commLoop()
{
    sensor.update();

    static uint32_t nextCheck = 0;
    static uint8_t skippedSends = 0;

    if (rtcTime() >= nextCheck)
    {
        static int16_t lastTemperature = 0xFFFF;
        static int16_t lastHumidity = 0xFFFF;
        static int16_t lastPressure = 0xFFFF;

        int16_t temperature = bme.readTemperature() * 256;
        int16_t humidity = bme.readHumidity() * 100;
        int16_t pressure = bme.readPressure() / 10;

        if (abs(temperature - lastTemperature) > 64 ||
            abs(humidity - lastHumidity) > 200 ||
            abs(pressure - lastPressure) > 30 ||
            skippedSends >= 30)
        {
            skippedSends = 0;
            lastTemperature = temperature;
            lastHumidity = humidity;
            lastPressure = pressure;

            sensor.powerUp();
            uint8_t msg[8] = {1};
            msg[1] = temperature >> 8;
            msg[2] = temperature;
            msg[3] = humidity >> 8;
            msg[4] = humidity;
            msg[5] = pressure >> 8;
            msg[6] = pressure;
            msg[7] = sensor.readVoltage() / 10 - 100;
            sensor.sendAndWait(msg, sizeof(msg));
            sensor.powerDown();
        }
        else
        {
            skippedSends++;
        }

        nextCheck = rtcTime() + 60;
    }

    return false;
}