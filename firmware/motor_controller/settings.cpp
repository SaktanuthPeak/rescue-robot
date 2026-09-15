#include "settings.h"

#include <EEPROM.h>
#include <stddef.h>

#include "robot_config.h"

namespace {
  constexpr uint16_t SETTINGS_MAGIC = 0x5242; // "RB"
  constexpr uint8_t SETTINGS_VERSION = 1;
  constexpr size_t SETTINGS_CRC_OFFSET = offsetof(PIDSettings, crc);

  uint16_t crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; ++i) {
      crc ^= (uint16_t)data[i] << 8;
      for (uint8_t bit = 0; bit < 8; ++bit) {
        crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
      }
    }

    return crc;
  }

  bool values_are_valid(const PIDSettings &settings) {
    // PID values outside this range are almost certainly corrupted EEPROM.
    // It also prevents an accidental bad value from producing extreme PWM.
    return settings.kp >= 0.0f && settings.kp <= 20.0f &&
           settings.ki >= 0.0f && settings.ki <= 20.0f &&
           settings.kd >= 0.0f && settings.kd <= 20.0f;
  }

  bool record_is_valid(const PIDSettings &settings) {
    return settings.magic == SETTINGS_MAGIC &&
           settings.version == SETTINGS_VERSION &&
           values_are_valid(settings) &&
           settings.crc == crc16((const uint8_t *)&settings, SETTINGS_CRC_OFFSET);
  }
}

void settings_set_defaults(PIDSettings *settings) {
  if (settings == nullptr)
    return;

  settings->magic = SETTINGS_MAGIC;
  settings->version = SETTINGS_VERSION;
  settings->kp = PID_KP;
  settings->ki = PID_KI;
  settings->kd = PID_KD;
  settings->crc = 0;
}

bool settings_load(PIDSettings *settings) {
  if (settings == nullptr)
    return false;

  PIDSettings stored;
  EEPROM.get(EEPROM_PID_SETTINGS_ADDRESS, stored);

  if (!record_is_valid(stored)) {
    settings_set_defaults(settings);
    return false;
  }

  *settings = stored;
  return true;
}

bool settings_save(const PIDSettings *settings) {
  if (settings == nullptr || !values_are_valid(*settings))
    return false;

  PIDSettings stored = *settings;
  stored.magic = SETTINGS_MAGIC;
  stored.version = SETTINGS_VERSION;
  stored.crc = crc16((const uint8_t *)&stored, SETTINGS_CRC_OFFSET);

  // EEPROM.put() on AVR uses update semantics, so unchanged bytes are not
  // rewritten. This function is only called from an explicit TM1638 SAVE key.
  EEPROM.put(EEPROM_PID_SETTINGS_ADDRESS, stored);

  PIDSettings verify;
  EEPROM.get(EEPROM_PID_SETTINGS_ADDRESS, verify);
  return record_is_valid(verify);
}
