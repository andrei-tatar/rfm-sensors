#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <sensor.h>
#include <rtc.h>
#include "comm.h"

static Adafruit_BME280 bme;
Sensor sensor(false);
bool thermostatStateReceived = false;
uint8_t thermostatState[3];

uint16_t readTemperature;
uint16_t readPressure;
uint16_t readhumidity;

void onMessageReceived(const uint8_t *data, uint8_t length, uint8_t rssi)
{
    if (length == 3 && data[0] == 2)
    {
        thermostatStateReceived = true;
        memcpy(thermostatState, data, 3);
    }
}

void commInit()
{
    sensor.init();
    sensor.powerLevel(31);
    sensor.powerDown();
    sensor.onMessage(onMessageReceived);
    bme.begin(0x76);
    bme.setSampling(
        Adafruit_BME280::sensor_mode::MODE_NORMAL,
        Adafruit_BME280::sensor_sampling::SAMPLING_X16,
        Adafruit_BME280::sensor_sampling::SAMPLING_X16,
        Adafruit_BME280::sensor_sampling::SAMPLING_X16,
        Adafruit_BME280::sensor_filter::FILTER_X4,
        Adafruit_BME280::standby_duration::STANDBY_MS_1000);
}

bool commSendState(ThermostatState &state)
{
    sensor.powerUp();
    uint8_t data[3];
    data[0] = 3;
    data[1] = state.on ? 1 : 0;
    data[2] = state.setpoint - 100;
    bool sent = sensor.sendAndWait(data, 3);
    sensor.powerDown();
    return sent;
}

bool commGetState(ThermostatState &state)
{
    sensor.powerUp();
    uint8_t requestState = 0x02;
    thermostatStateReceived = false;
    if (!sensor.sendAndWait(&requestState, 1))
    {
        sensor.powerDown();
        return false;
    }

    uint32_t start = millis();
    while (millis() - start < 1000 && !thermostatStateReceived)
    {
        sensor.update();
    }

    sensor.powerDown();
    if (thermostatStateReceived)
    {
        state.on = thermostatState[1] == 1;
        state.setpoint = thermostatState[2] + 100;
        return true;
    }

    return false;
}

bool commLoop()
{
    sensor.update();

    static uint32_t nextCheck = 0;
    static uint8_t skippedSends = 0;

    if (rtcTime() >= nextCheck)
    {
        static int16_t lastTemperature = 0;
        static int16_t lastHumidity = 0;
        static int16_t lastPressure = 0;

        float t = bme.readTemperature();
        float h = bme.readHumidity();
        float p = bme.readPressure();

        readTemperature = t * 10;
        readhumidity = h * 10;
        readPressure = p / 100;

        int16_t temperature = t * 256;
        int16_t humidity = h * 100;
        int16_t pressure = p / 10;

        if (temperature == 0)
        {
            // TODO: sometimes temperature is sent as 0
            return false;
        }

        if (abs(temperature - lastTemperature) > 25 ||
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