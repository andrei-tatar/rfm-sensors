#include "Arduino.h"
#include "MAX44009.h"
#include "Wire.h"

static const uint8_t    I2C_ADDRESS = (0x4A);

static const uint8_t    INT_STATUS_REG = (0x00);
static const uint8_t    INT_ENABLE_REG = (0x01);
static const uint8_t    CONFIGURATION_REG = (0x02);
static const uint8_t        CONFIG_CONT_MASK = (bit(7));  // CONTINOUS MODE
static const uint8_t            CONFIG_CONT_ON = (1 << 7);
static const uint8_t            CONFIG_CONT_OFF = (0);
static const uint8_t        CONFIG_MANUAL_MASK = (bit(6));  // MANUAL Set CDR and TIME
static const uint8_t            CONFIG_MANUAL_ON = (1 << 6);
static const uint8_t            CONFIG_MANUAL_OFF = (0);
static const uint8_t        CONFIG_CDR_MASK = (bit(3));  // Current DIVISION RATIO (When HIGH Brightness --> Current/8
static const uint8_t            CONFIG_CDR_1div1 = (0);
static const uint8_t            CONFIG_CDR_1div8 = (1 << 3);
static const uint8_t        CONFIG_TIM_MASK = (bit(2) | bit(1) | bit(0));
static const uint8_t            CONFIG_TIM_800MS = (0);
static const uint8_t            CONFIG_TIM_400MS = (1);
static const uint8_t            CONFIG_TIM_200MS = (2);
static const uint8_t            CONFIG_TIM_100MS = (3);
static const uint8_t            CONFIG_TIM_50MS = (4);
static const uint8_t            CONFIG_TIM_25MS = (5);
static const uint8_t            CONFIG_TIM_12MS = (6);
static const uint8_t            CONFIG_TIM_6MS = (7);

static const uint8_t    LUX_HIGH_REG = (0x03);
static const uint8_t    LUX_LOW_REG = (0x04);
static const uint8_t    THRESHOLD_UPPER_REG = (0x05);
static const uint8_t    THRESHOLD_LOWER_REG = (0x06);
static const uint8_t    THRESHOLD_TIMER_REG = (0x07);

void MAX44009::setEnabled(const uint8_t enable)
{
    uint8_t _value;
    if (enable)
    {
        _value = CONFIG_CONT_ON;
        setRegister(I2C_ADDRESS, INT_ENABLE_REG, 1, 0);
        setRegister(I2C_ADDRESS, CONFIGURATION_REG, CONFIG_MANUAL_MASK, CONFIG_MANUAL_OFF);
    }
    else
    {
        _value = CONFIG_CONT_OFF;
    }
    setRegister(I2C_ADDRESS, CONFIGURATION_REG, CONFIG_CONT_MASK, _value);
};

uint8_t MAX44009::initialize()
{
    if (probe(I2C_ADDRESS) == 0) return 0;

    setEnabled(1);
    return 1;
}

/**
READING only high-register:
Lux = 2(exponent) x mantissa x 0.72
Exponent = 8xE3 + 4xE2 + 2xE1 + E0
Mantissa = 8xM7 + 4xM6 + 2xM5 + M4

READING combined Registers:
E3�E0: Exponent bits of lux reading
M7�M0: Mantissa byte of lux reading
Lux = 2(exponent) x mantissa x 0.045
*/
uint16_t MAX44009::getMeasurement()
{
    uint8_t reg[2];
    read(I2C_ADDRESS, LUX_HIGH_REG, reg, 2);

    return ((uint16_t)reg[0] << 8) | reg[1];
}

void MAX44009::setRegister(const uint8_t address, const uint8_t registeraddress, const uint8_t mask, const uint8_t writevalue)
{
    uint8_t _setting;
    read(address, registeraddress, &_setting, 1);
    _setting &= ~mask;
    _setting |= (writevalue&mask);
    writeByte(address, registeraddress, _setting);
}

void read(const uint8_t address, const uint8_t registeraddress, uint8_t buff[], const uint8_t length)
{
    Wire.beginTransmission(address); 	// Adress + WRITE (0)
    Wire.write(registeraddress);
    Wire.endTransmission(false); 		// No Stop Condition, for repeated Talk

    if (!length) return;
    Wire.requestFrom(address, length); 	// Address + READ (1)
    uint8_t _i;
    _i = 0;
    while (Wire.available())
    {
        buff[_i] = Wire.read();
        _i++;
    }
}

uint8_t MAX44009::probe(const uint8_t address)
{
    Wire.beginTransmission(address);
    if (Wire.endTransmission(true) == 0)
        return 1; // found something
    else
        return 0; // no response
}

void MAX44009::writeByte(const uint8_t address, const uint8_t register_address, const uint8_t write_value)
{
    Wire.beginTransmission(address);
    Wire.write(register_address);
    Wire.write(write_value);
    Wire.endTransmission(true);
}

void MAX44009::read(const uint8_t address, const uint8_t registeraddress, uint8_t buff[], const uint8_t length)
{
    Wire.beginTransmission(address); 	// Adress + WRITE (0)
    Wire.write(registeraddress);
    Wire.endTransmission(false); 		// No Stop Condition, for repeated Talk

    if (!length) return;
    Wire.requestFrom(address, length); 	// Address + READ (1)
    uint8_t _i;
    _i = 0;
    while (Wire.available())
    {
        buff[_i] = Wire.read();
        _i++;
    }
}
