#ifndef MOTOR_CONTROLLER_MEGA_BATTERY_SENSOR_H
#define MOTOR_CONTROLLER_MEGA_BATTERY_SENSOR_H

#include <Arduino.h>

struct BatterySample
{
    uint16_t adc;
    uint32_t millivolts;
};

void battery_init();
BatterySample battery_read();

#endif
