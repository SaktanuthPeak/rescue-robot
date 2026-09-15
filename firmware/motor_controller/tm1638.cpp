#include "tm1638.h"

TM1638::TM1638(uint8_t stbPin, uint8_t clkPin, uint8_t dioPin)
    : stbPin_(stbPin), clkPin_(clkPin), dioPin_(dioPin) {}

void TM1638::begin(uint8_t brightness) {
  pinMode(stbPin_, OUTPUT);
  pinMode(clkPin_, OUTPUT);
  pinMode(dioPin_, OUTPUT);

  digitalWrite(stbPin_, HIGH);
  digitalWrite(clkPin_, HIGH);
  digitalWrite(dioPin_, HIGH);

  sendCommand(0x40); // automatic address increment
  sendCommand((uint8_t)(0x88 | (brightness & 0x07))); // display on
  displayText("        ");
}

void TM1638::sendCommand(uint8_t command) {
  digitalWrite(stbPin_, LOW);
  sendByte(command);
  digitalWrite(stbPin_, HIGH);
}

void TM1638::sendByte(uint8_t value) {
  for (uint8_t bit = 0; bit < 8; ++bit) {
    digitalWrite(clkPin_, LOW);
    digitalWrite(dioPin_, (value & 0x01) ? HIGH : LOW);
    value >>= 1;
    digitalWrite(clkPin_, HIGH);
  }
}

uint8_t TM1638::readByte() {
  uint8_t value = 0;

  for (uint8_t bit = 0; bit < 8; ++bit) {
    digitalWrite(clkPin_, LOW);
    if (digitalRead(dioPin_) == HIGH)
      value |= (uint8_t)(1 << bit);
    digitalWrite(clkPin_, HIGH);
  }

  return value;
}

uint32_t TM1638::readButtons() {
  uint32_t buttons = 0;

  digitalWrite(stbPin_, LOW);
  sendByte(0x42); // read key scan data
  pinMode(dioPin_, INPUT);

  for (uint8_t byteIndex = 0; byteIndex < 4; ++byteIndex)
    buttons |= (uint32_t)readByte() << (byteIndex * 8);

  pinMode(dioPin_, OUTPUT);
  digitalWrite(dioPin_, HIGH);
  digitalWrite(stbPin_, HIGH);
  return buttons;
}

uint8_t TM1638::encodeChar(char value) {
  switch (value) {
    case '0': return 0x3F;
    case '1': return 0x06;
    case '2': return 0x5B;
    case '3': return 0x4F;
    case '4': return 0x66;
    case '5': return 0x6D;
    case '6': return 0x7D;
    case '7': return 0x07;
    case '8': return 0x7F;
    case '9': return 0x6F;
    case 'A': case 'a': return 0x77;
    case 'B': case 'b': return 0x7C;
    case 'C': case 'c': return 0x39;
    case 'D': case 'd': return 0x5E;
    case 'E': case 'e': return 0x79;
    case 'F': case 'f': return 0x71;
    case 'G': case 'g': return 0x3D;
    case 'I': case 'i': return 0x06;
    case 'K': case 'k': return 0x76;
    case 'L': case 'l': return 0x38;
    case 'N': case 'n': return 0x54;
    case 'O': case 'o': return 0x3F;
    case 'P': case 'p': return 0x73;
    case 'R': case 'r': return 0x50;
    case 'S': case 's': return 0x6D;
    case 'T': case 't': return 0x78;
    case 'U': case 'u': return 0x3E;
    case '-': return 0x40;
    default: return 0x00;
  }
}

void TM1638::displayText(const char *text, uint8_t ledMask) {
  uint8_t segments[8] = {0};
  uint8_t displayIndex = 0;

  if (text != nullptr) {
    for (uint8_t i = 0; text[i] != '\0' && displayIndex < 8; ++i) {
      if (text[i] == '.') {
        if (displayIndex > 0)
          segments[displayIndex - 1] |= 0x80;
        continue;
      }
      segments[displayIndex++] = encodeChar(text[i]);
    }
  }

  // Fixed-address mode makes each digit/LED pair explicit and predictable.
  digitalWrite(stbPin_, LOW);
  sendByte(0x44);
  digitalWrite(stbPin_, HIGH);

  digitalWrite(stbPin_, LOW);
  sendByte(0xC0);
  for (uint8_t i = 0; i < 8; ++i) {
    sendByte(segments[i]);
    sendByte((ledMask & (uint8_t)(1 << i)) ? 0x01 : 0x00);
  }
  digitalWrite(stbPin_, HIGH);
}
