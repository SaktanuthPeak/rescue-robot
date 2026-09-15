#ifndef MOTOR_CONTROLLER_TM1638_H
#define MOTOR_CONTROLLER_TM1638_H

#include <Arduino.h>

// Small dependency-free TM1638 driver. The module uses three wires:
// STB (strobe), CLK (clock), and DIO (data).
class TM1638 {
public:
  TM1638(uint8_t stbPin, uint8_t clkPin, uint8_t dioPin);

  void begin(uint8_t brightness = 2);
  uint32_t readButtons();
  void displayText(const char *text, uint8_t ledMask = 0);

private:
  uint8_t stbPin_;
  uint8_t clkPin_;
  uint8_t dioPin_;

  void sendCommand(uint8_t command);
  void sendByte(uint8_t value);
  uint8_t readByte();
  static uint8_t encodeChar(char value);
};

#endif
