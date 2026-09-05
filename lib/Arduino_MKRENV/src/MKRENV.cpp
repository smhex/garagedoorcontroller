/*
  This file is part of the Arduino_MKRENV library.
  Copyright (c) 2019 Arduino SA. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <Wire.h>
#include <math.h>

#include "MKRENV.h"

#define HTS221_ADDRESS   0x5F
#define LPS22HB_ADDRESS  0x5C
#define VEML6075_ADDRESS 0x10

#define LIGHT_SENSOR_PIN A2

#define HTS221_WHO_AM_I_REG         0x0f
#define HTS221_CTRL1_REG            0x20
#define HTS221_CTRL2_REG            0x21
#define HTS221_CTRL3_REG            0x22
#define HTS221_STATUS_REG           0x27
#define HTS221_HUMIDITY_OUT_L_REG   0x28
#define HTS221_TEMP_OUT_L_REG       0x2a
#define HTS221_H0_rH_x2_REG         0x30
#define HTS221_H1_rH_x2_REG         0x31
#define HTS221_T0_degC_x8_REG       0x32
#define HTS221_T1_degC_x8_REG       0x33
#define HTS221_T1_T0_MSB_REG        0x35
#define HTS221_H0_T0_OUT_REG        0x36
#define HTS221_H1_T0_OUT_REG        0x3a
#define HTS221_T0_OUT_REG           0x3c
#define HTS221_T1_OUT_REG           0x3e

#define LPS22HB_WHO_AM_I_REG        0x0f
#define LPS22HB_CTRL2_REG           0x11
#define LPS22HB_STATUS_REG          0x27
#define LPS22HB_PRESS_OUT_XL_REG    0x28
#define LPS22HB_PRESS_OUT_L_REG     0x29
#define LPS22HB_PRESS_OUT_H_REG     0x2a

#define VEML6075_UV_CONF_REG        0x00
#define VEML6075_UVA_DATA_REG       0x07
#define VEML6075_UVB_DATA_REG       0x09
#define VEML6075_UVCOMP1_REG        0x0a
#define VEML6075_UVCOMP2_REG        0x0b
#define VEML6075_ID_REG             0x0c

ENVClass::ENVClass(TwoWire  & wire, int lightSensorPin) :
  _wire(&wire),
  _lightSensorPin(lightSensorPin)
{
}

int ENVClass::begin()
{
  _ioFailed = false;
  _wire->begin();

  if (i2cRead(HTS221_ADDRESS, HTS221_WHO_AM_I_REG) != 0xbc) {

    return 0;
  }

  if (i2cRead(LPS22HB_ADDRESS, LPS22HB_WHO_AM_I_REG) != 0xb1) {
    return 0;
  }

  if (i2cReadWord(VEML6075_ADDRESS, VEML6075_ID_REG) != 0x0026) {
    _isv2 = true;
  }

  readHTS221Calibration();

  // turn on the HTS221 and enable Block Data Update
  i2cWrite(HTS221_ADDRESS, HTS221_CTRL1_REG, 0x84);

  // Disable HTS221_DRDY by default and make the output open drain
  // This allows to use pin D6 for other purposes (e.g. LED_BUILTIN on the Arduino MKR WAN 1300)
  i2cWrite(HTS221_ADDRESS, HTS221_CTRL3_REG, 0x40);

  // configure VEML6075 for 100 ms
  i2cWriteWord(VEML6075_ADDRESS, VEML6075_UV_CONF_REG, 0x0010);

  return !_ioFailed && isfinite(_hts221TemperatureSlope) && isfinite(_hts221TemperatureZero)
      && isfinite(_hts221HumiditySlope) && isfinite(_hts221HumidityZero);
}

// Bound conversion polling. Individual Wire transactions still use core behavior.
bool ENVClass::waitReady(uint8_t address, uint8_t reg, uint8_t mask, uint8_t expected)
{
  const unsigned long started = millis();
  do {
    const int value = i2cRead(address, reg);
    if (value < 0) return false;
    if ((value & mask) == expected) return true;
    yield();
  } while (millis() - started < 250);
  return false;
}

void ENVClass::end()
{
  // shutdown VEML6075
  i2cWriteWord(VEML6075_ADDRESS, VEML6075_UV_CONF_REG, 0x0001);

  // refresh the content of HTS221's internal registers via BOOT bit
  i2cWrite(HTS221_ADDRESS, HTS221_CTRL2_REG, 0x80);

  // disable HTS221
  i2cWrite(HTS221_ADDRESS, HTS221_CTRL1_REG, 0x00);

  _wire->end();

  // restore the light sensor pin to input mode
  pinMode(LIGHT_SENSOR_PIN, INPUT);
}

float ENVClass::readTemperature(int units)
{
  _ioFailed = false;
  // Wait for ONE_SHOT bit to be cleared by the hardware
  if (!waitReady(HTS221_ADDRESS, HTS221_CTRL2_REG, 1, 0)) return NAN;

  // trigger one shot
  if (!i2cWrite(HTS221_ADDRESS, HTS221_CTRL2_REG, 0x01)) return NAN;

  // wait for completion
  if (!waitReady(HTS221_ADDRESS, HTS221_STATUS_REG, 1, 1)) return NAN;

  // read value and convert
  int16_t tout = i2cRead16(HTS221_ADDRESS, HTS221_TEMP_OUT_L_REG);
  if (_ioFailed) return NAN;
  float reading = (tout * _hts221TemperatureSlope + _hts221TemperatureZero);
  if (units == FAHRENHEIT) { // Fahrenheit = (Celsius * 9 / 5) + 32
    return (reading * 9.0 / 5.0) + 32.0;
  } else {
    return reading;
  }
}

float ENVClass::readHumidity()
{
  _ioFailed = false;
  //Wait for ONE_SHOT bit to be cleared by the hardware
  if (!waitReady(HTS221_ADDRESS, HTS221_CTRL2_REG, 1, 0)) return NAN;

  // trigger one shot
  if (!i2cWrite(HTS221_ADDRESS, HTS221_CTRL2_REG, 0x01)) return NAN;

  // wait for completion
  if (!waitReady(HTS221_ADDRESS, HTS221_STATUS_REG, 2, 2)) return NAN;

  // read value and convert
  int16_t hout = i2cRead16(HTS221_ADDRESS, HTS221_HUMIDITY_OUT_L_REG);
  if (_ioFailed) return NAN;

  return (hout * _hts221HumiditySlope + _hts221HumidityZero);
}

float ENVClass::readPressure(int units)
{
  _ioFailed = false;
  // trigger one shot
  if (!i2cWrite(LPS22HB_ADDRESS, LPS22HB_CTRL2_REG, 0x01)) return NAN;

  // wait for ONE_SHOT bit to be cleared by the hardware
  if (!waitReady(LPS22HB_ADDRESS, LPS22HB_CTRL2_REG, 1, 0)) return NAN;

  const int low = i2cRead(LPS22HB_ADDRESS, LPS22HB_PRESS_OUT_XL_REG);
  const int mid = i2cRead(LPS22HB_ADDRESS, LPS22HB_PRESS_OUT_L_REG);
  const int high = i2cRead(LPS22HB_ADDRESS, LPS22HB_PRESS_OUT_H_REG);
  if (_ioFailed) return NAN;
  float reading = (low | (mid << 8) | (high << 16)) / 40960.0;

  if (units == MILLIBAR) { // 1 kPa = 10 millibar
    return reading * 10;
  } else if (units == PSI) {  // 1 kPa = 0.145038 PSI
    return reading * 0.145038;
  } else {
    return reading;
  }
}

float ENVClass::readIlluminance(int units)
{
  // read analog value and convert to mV
  float mV = (analogRead(_lightSensorPin) * 3300.0) / 1023.0;

  // 5 mV per lux
  float reading = (mV / 5.0); // Readings are in lux scale
  if (units == FOOTCANDLE) { // 1 lux = 0.092903 foot-candle
    return reading * 0.092903;
  } else {
    return reading; // 1 lux = 1 meter-candle
  }
}

// UV formulas and constants based on:
//   https://www.vishay.com/docs/84339/designingveml6075.pdf

float ENVClass::readUVA()
{
  const float a = 2.22;
  const float b = 1.33;

  // read UVA and UV COMP's, then calculate compensated value
  uint16_t uva = i2cReadWord(VEML6075_ADDRESS, VEML6075_UVA_DATA_REG);
  uint16_t uvcomp1 = i2cReadWord(VEML6075_ADDRESS, VEML6075_UVCOMP1_REG);
  uint16_t uvcomp2 = i2cReadWord(VEML6075_ADDRESS, VEML6075_UVCOMP2_REG);

  float uvaComp = uva - (a * uvcomp1) - (b * uvcomp2);

  return uvaComp;
}

float ENVClass::readUVB()
{
  const float c = 2.95;
  const float d = 1.74;

  // read UVB and UV COMP's, then calculate compensated value
  uint16_t uvb = i2cReadWord(VEML6075_ADDRESS, VEML6075_UVB_DATA_REG);
  uint16_t uvcomp1 = i2cReadWord(VEML6075_ADDRESS, VEML6075_UVCOMP1_REG);
  uint16_t uvcomp2 = i2cReadWord(VEML6075_ADDRESS, VEML6075_UVCOMP2_REG);

  float uvbComp = uvb - (c * uvcomp1) - (d * uvcomp2);

  return uvbComp;
}

float ENVClass::readUVIndex()
{
  const float UVAresp = 0.001461;
  const float UVBresp = 0.002591;

  float uva = readUVA();
  float uvb = readUVB();

  float uvi = ((uva * UVAresp) + (uvb * UVBresp)) / 2.0;

  return uvi;
}

int ENVClass::i2cRead(uint8_t address, uint8_t reg)
{
  if (_isv2 && address == VEML6075_ADDRESS) {
    return 0;
  }
  _wire->beginTransmission(address);
  _wire->write(reg);
  if (_wire->endTransmission(false) != 0) {
    _ioFailed = true;
    return -1;
  }

  if (_wire->requestFrom(address, 1) != 1) {
    _ioFailed = true;
    return -1;
  }
  
  return _wire->read();
}

int ENVClass::i2cWrite(uint8_t address, uint8_t reg, uint8_t val)
{
  if (_isv2 && address == VEML6075_ADDRESS) {
    return 0;
  }
  _wire->beginTransmission(address);
  _wire->write(reg);
  _wire->write(val);
  if (_wire->endTransmission() != 0) {
    _ioFailed = true;
    return 0;
  }

  return 1;
}

int ENVClass::i2cReadWord(uint8_t address, uint8_t reg)
{
  if (_isv2 && address == VEML6075_ADDRESS) {
    return 0;
  }
  _wire->beginTransmission(address);
  _wire->write(reg);
  if (_wire->endTransmission(false) != 0) {
    return -1;
  }

  if (_wire->requestFrom(address, 2) != 2) {
    return -1;
  }

  return (_wire->read() | _wire->read() << 8);
}

int ENVClass::i2cWriteWord(uint8_t address, uint8_t reg, uint16_t val)
{
  if (_isv2 && address == VEML6075_ADDRESS) {
    return 1;
  }
  _wire->beginTransmission(address);
  _wire->write(reg);
  _wire->write(val & 0xff);
  _wire->write(val >> 8);
  if (_wire->endTransmission() != 0) {
    return 0;
  }

  return 1;
}

void ENVClass::readHTS221Calibration()
{
  uint8_t h0rH = i2cRead(HTS221_ADDRESS, HTS221_H0_rH_x2_REG);
  uint8_t h1rH = i2cRead(HTS221_ADDRESS, HTS221_H1_rH_x2_REG);

  uint16_t t0degC = i2cRead(HTS221_ADDRESS, HTS221_T0_degC_x8_REG) | ((i2cRead(HTS221_ADDRESS, HTS221_T1_T0_MSB_REG) & 0x03) << 8);
  uint16_t t1degC = i2cRead(HTS221_ADDRESS, HTS221_T1_degC_x8_REG) | ((i2cRead(HTS221_ADDRESS, HTS221_T1_T0_MSB_REG) & 0x0c) << 6);

  int16_t h0t0Out = i2cRead16(HTS221_ADDRESS, HTS221_H0_T0_OUT_REG);
  int16_t h1t0Out = i2cRead16(HTS221_ADDRESS, HTS221_H1_T0_OUT_REG);

  int16_t t0Out = i2cRead16(HTS221_ADDRESS, HTS221_T0_OUT_REG);
  int16_t t1Out = i2cRead16(HTS221_ADDRESS, HTS221_T1_OUT_REG);

  // calculate slopes and 0 offset from calibration values,
  // for future calculations: value = a * X + b

  _hts221HumiditySlope = (h1rH - h0rH) / (2.0 * (h1t0Out - h0t0Out));
  _hts221HumidityZero = (h0rH / 2.0) - _hts221HumiditySlope * h0t0Out;

  _hts221TemperatureSlope = (t1degC - t0degC) / (8.0 * (t1Out - t0Out));
  _hts221TemperatureZero = (t0degC / 8.0) - _hts221TemperatureSlope * t0Out;
}

ENVClass ENV(Wire, LIGHT_SENSOR_PIN);
