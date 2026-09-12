#include "PCA9685_Control.h"
#include <math.h>

#define MODE1_REG 0x00
#define PRESCALE_REG 0xFE
#define LED0_ON_L 0x06

PCA9685Control::PCA9685Control(uint8_t addr) {
  _i2caddr = addr;
  for (uint8_t channel = 0; channel < 16; channel++) {
    _currentPos[channel] = 0;
  }
}

void PCA9685Control::begin() {
  Wire.begin();
  writeRegister(MODE1_REG, 0x00);
}

void PCA9685Control::writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(_i2caddr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t PCA9685Control::readRegister(uint8_t reg) {
  Wire.beginTransmission(_i2caddr);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(static_cast<int>(_i2caddr), 1);
  return Wire.available() ? Wire.read() : 0;
}

void PCA9685Control::setPWMFreq(float freq) {
  freq *= 0.9;
  float prescaleValue = 25000000.0 / 4096.0 / freq - 1.0;
  uint8_t prescale = static_cast<uint8_t>(floor(prescaleValue + 0.5));

  uint8_t oldmode = readRegister(MODE1_REG);
  uint8_t sleepMode = (oldmode & 0x7F) | 0x10;

  writeRegister(MODE1_REG, sleepMode);
  writeRegister(PRESCALE_REG, prescale);
  writeRegister(MODE1_REG, oldmode);
  delay(5);
  writeRegister(MODE1_REG, oldmode | 0xA0);
}

void PCA9685Control::setPWM(uint8_t channel, uint16_t on, uint16_t off) {
  if (channel >= 16) return;

  Wire.beginTransmission(_i2caddr);
  Wire.write(LED0_ON_L + 4 * channel);
  Wire.write(on & 0xFF);
  Wire.write(on >> 8);
  Wire.write(off & 0xFF);
  Wire.write(off >> 8);
  Wire.endTransmission();
  _currentPos[channel] = off;
}

uint16_t PCA9685Control::getPos(uint8_t channel) {
  return channel < 16 ? _currentPos[channel] : 0;
}
