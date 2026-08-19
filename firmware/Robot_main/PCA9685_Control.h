#ifndef PCA9685_CONTROL_H
#define PCA9685_CONTROL_H

#include <Arduino.h>
#include <Wire.h>

class PCA9685Control {
  private:
    uint8_t _i2caddr;
    // เพิ่มอาเรย์เก็บค่าตำแหน่งปัจจุบันของ Servo ทั้ง 16 ช่อง
    uint16_t _currentPos[16]; 
    
    void writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);

  public:
    PCA9685Control(uint8_t addr = 0x40);
    
    void begin();
    void setPWMFreq(float freq);
    void setPWM(uint8_t channel, uint16_t on, uint16_t off);
    
    // เพิ่มฟังก์ชันใหม่: สำหรับดึงค่าตำแหน่งปัจจุบัน
    uint16_t getPos(uint8_t channel);
};

#endif
