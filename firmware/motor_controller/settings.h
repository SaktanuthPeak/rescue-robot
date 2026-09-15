#ifndef MOTOR_CONTROLLER_SETTINGS_H
#define MOTOR_CONTROLLER_SETTINGS_H

#include <Arduino.h>

// Persistent settings stored in the Mega 2560's internal EEPROM.
// The magic/version/CRC fields make old or corrupted EEPROM data fall back
// to the compile-time defaults in robot_config.h.
struct PIDSettings {
  uint16_t magic;
  uint8_t version;
  float kp;
  float ki;
  float kd;
  uint16_t crc;
};

void settings_set_defaults(PIDSettings *settings);
bool settings_load(PIDSettings *settings);
bool settings_save(const PIDSettings *settings);

#endif
