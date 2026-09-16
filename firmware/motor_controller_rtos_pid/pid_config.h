#ifndef PID_CONFIG_H
#define PID_CONFIG_H

#include <Arduino.h>

struct PidConfig {
    float kp;
    float ki;
    float kd;
};

PidConfig pid_config_defaults();
void pid_config_clamp(PidConfig &config);
bool pid_config_is_valid(const PidConfig &config);
bool pid_config_load(PidConfig &config);
bool pid_config_save(const PidConfig &config);

#endif
