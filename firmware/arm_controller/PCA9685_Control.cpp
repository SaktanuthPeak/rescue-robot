#include "PCA9685_Control.h"

// Register ที่สำคัญภายในชิป PCA9685
#define MODE1_REG    0x00
#define PRESCALE_REG 0xFE
#define LED0_ON_L    0x06

PCA9685Control::PCA9685Control(uint8_t addr) {
  _i2caddr = addr;
}

void PCA9685Control::begin() {
  Wire.begin();
  writeRegister(MODE1_REG, 0x00); // Reset การตั้งค่า
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
  Wire.requestFrom((int)_i2caddr, 1);
  return Wire.read();
}

void PCA9685Control::setPWMFreq(float freq) {
  freq *= 0.9;  // ชดเชยความคลาดเคลื่อนของ Oscillator ภายในชิป
  float prescaleval = 25000000.0 / 4096.0 / freq - 1.0;
  uint8_t prescale = floor(prescaleval + 0.5);

  uint8_t oldmode = readRegister(MODE1_REG);
  uint8_t newmode = (oldmode & 0x7F) | 0x10; // เข้าโหมด Sleep เพื่อตั้งค่า Prescale
  
  writeRegister(MODE1_REG, newmode);     // สั่ง Sleep
  writeRegister(PRESCALE_REG, prescale); // ใส่ค่าความถี่
  writeRegister(MODE1_REG, oldmode);     // ตื่นจาก Sleep
  delay(5);
  writeRegister(MODE1_REG, oldmode | 0xA0); // เปิด Auto-Increment
}

void PCA9685Control::setPWM(uint8_t channel, uint16_t on, uint16_t off) {
  Wire.beginTransmission(_i2caddr);
  Wire.write(LED0_ON_L + 4 * channel); // คำนวณหาตำแหน่ง Register ของช่องนั้นๆ
  Wire.write(on & 0xFF);               // ON_L
  Wire.write(on >> 8);                 // ON_H
  Wire.write(off & 0xFF);              // OFF_L
  Wire.write(off >> 8);                // OFF_H
  Wire.endTransmission();
}

// ฟังก์ชันใหม่: รีเทิร์นค่าตำแหน่งล่าสุดกลับไปให้ผู้ใช้งาน
uint16_t PCA9685Control::getPos(uint8_t channel) {
  return _currentPos[channel];
}
