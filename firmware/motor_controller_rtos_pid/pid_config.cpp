#include "pid_config.h"

#include <EEPROM.h>
#include <stddef.h>

#include "robot_config.h"

namespace {
constexpr uint16_t PID_EEPROM_MAGIC = 0x5049; // "PI"
constexpr uint8_t PID_EEPROM_VERSION = 1;

struct StoredPidConfig {
    uint16_t magic;
    uint8_t version;
    uint8_t reserved;
    PidConfig config;
    uint16_t crc;
};

uint16_t crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;

    while (length-- > 0) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) ? (crc >> 1) ^ 0xA001U : (crc >> 1);
        }
    }

    return crc;
}

uint16_t config_crc(const StoredPidConfig &record) {
    return crc16(reinterpret_cast<const uint8_t *>(&record), offsetof(StoredPidConfig, crc));
}

bool valid_gain(float value, float minimum, float maximum) {
    // value == value rejects NaN without requiring a floating-point library.
    return value == value && value >= minimum && value <= maximum;
}
} // namespace

PidConfig pid_config_defaults() {
    return {PID_KP, PID_KI, PID_KD};
}

void pid_config_clamp(PidConfig &config) {
    config.kp = constrain(config.kp, PID_KP_MIN, PID_KP_MAX);
    config.ki = constrain(config.ki, PID_KI_MIN, PID_KI_MAX);
    config.kd = constrain(config.kd, PID_KD_MIN, PID_KD_MAX);
}

bool pid_config_is_valid(const PidConfig &config) {
    return valid_gain(config.kp, PID_KP_MIN, PID_KP_MAX) &&
           valid_gain(config.ki, PID_KI_MIN, PID_KI_MAX) &&
           valid_gain(config.kd, PID_KD_MIN, PID_KD_MAX);
}

bool pid_config_load(PidConfig &config) {
    if (PID_EEPROM_ADDRESS < 0 ||
        (size_t)PID_EEPROM_ADDRESS + sizeof(StoredPidConfig) > (size_t)EEPROM.length()) {
        config = pid_config_defaults();
        return false;
    }

    StoredPidConfig record = {};
    EEPROM.get(PID_EEPROM_ADDRESS, record);

    if (record.magic != PID_EEPROM_MAGIC ||
        record.version != PID_EEPROM_VERSION ||
        record.crc != config_crc(record) ||
        !pid_config_is_valid(record.config)) {
        config = pid_config_defaults();
        return false;
    }

    config = record.config;
    return true;
}

bool pid_config_save(const PidConfig &config) {
    if (!pid_config_is_valid(config) ||
        PID_EEPROM_ADDRESS < 0 ||
        (size_t)PID_EEPROM_ADDRESS + sizeof(StoredPidConfig) > (size_t)EEPROM.length()) {
        return false;
    }

    StoredPidConfig record = {};
    record.magic = PID_EEPROM_MAGIC;
    record.version = PID_EEPROM_VERSION;
    record.reserved = 0;
    record.config = config;
    record.crc = config_crc(record);

    EEPROM.put(PID_EEPROM_ADDRESS, record);
    return true;
}
